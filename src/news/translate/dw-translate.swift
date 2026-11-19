// dw-translate: on-device translation helper for Deepwater (Apple Translation framework, macOS 26+).
//
//   dw-translate <source> <target>        e.g. dw-translate en fr
//
// Protocol (one JSON object per line, UTF-8):
//   first output line   {"status":"installed"|"supported"|"unsupported"}
//                       "supported" = the language pack is not downloaded (System Settings › General ›
//                       Language & Region › Translation Languages); nothing is translated then.
//   input line          {"id":7,"text":"The Fed holds rates"}
//   output line         {"id":7,"text":"La Fed maintient ses taux"}   or   {"id":7,"error":"…"}
// Everything runs on device; no text leaves the Mac.
import Foundation
import Translation

func emit(_ obj: [String: Any]) {
  if let d = try? JSONSerialization.data(withJSONObject: obj), var s = String(data: d, encoding: .utf8) {
    s += "\n"
    FileHandle.standardOutput.write(s.data(using: .utf8)!)
  }
}

@main struct Main {
  static func main() async {
    let args = CommandLine.arguments
    let src = Locale.Language(identifier: args.count > 1 ? args[1] : "en")
    let dst = Locale.Language(identifier: args.count > 2 ? args[2] : "fr")
    let status = await LanguageAvailability().status(from: src, to: dst)
    switch status {
    case .installed: emit(["status": "installed"])
    case .supported: emit(["status": "supported"])
    default: emit(["status": "unsupported"])
    }
    let session: TranslationSession? = status == .installed ? TranslationSession(installedSource: src, target: dst) : nil
    do {
      for try await line in FileHandle.standardInput.bytes.lines {
        guard let data = line.data(using: .utf8),
              let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let id = obj["id"] as? Int, let text = obj["text"] as? String else { continue }
        guard let session else { emit(["id": id, "error": "language pack not installed"]); continue }
        do {
          let r = try await session.translate(text)
          emit(["id": id, "text": r.targetText])
        } catch {
          emit(["id": id, "error": "\(error)"])
        }
      }
    } catch {}
  }
}
