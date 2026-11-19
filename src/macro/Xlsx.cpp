#include "macro/Xlsx.h"
#include <QXmlStreamReader>
#include <QtEndian>

namespace macro::xlsx {
namespace {
// ---- raw DEFLATE (after M. Adler's puff.c, simplified)
struct State {
  const uchar* in; qsizetype len; qsizetype pos = 0;
  quint64 bitbuf = 0; int bitcnt = 0; bool err = false;
  QByteArray out;
};
int bits(State& s, int need) {
  quint64 val = s.bitbuf;
  while (s.bitcnt < need) {
    if (s.pos >= s.len) { s.err = true; return 0; }
    val |= quint64(s.in[s.pos++]) << s.bitcnt;
    s.bitcnt += 8;
  }
  s.bitbuf = val >> need;
  s.bitcnt -= need;
  return int(val & ((quint64(1) << need) - 1));
}
struct Huff { short count[16]; short symbol[320]; };
int decode(State& s, const Huff& h) {
  int code = 0, first = 0, index = 0;
  for (int len = 1; len <= 15; ++len) {
    code |= bits(s, 1);
    if (s.err) return -1;
    const int count = h.count[len];
    if (code - count < first) return h.symbol[index + (code - first)];
    index += count; first += count; first <<= 1; code <<= 1;
  }
  return -1;
}
int construct(Huff& h, const short* length, int n) {
  for (int l = 0; l <= 15; ++l) h.count[l] = 0;
  for (int sym = 0; sym < n; ++sym) h.count[length[sym]]++;
  if (h.count[0] == n) return 0;
  int left = 1;
  for (int l = 1; l <= 15; ++l) { left <<= 1; left -= h.count[l]; if (left < 0) return left; }
  short offs[16]; offs[1] = 0;
  for (int l = 1; l < 15; ++l) offs[l + 1] = offs[l] + h.count[l];
  for (int sym = 0; sym < n; ++sym) if (length[sym] != 0) h.symbol[offs[length[sym]]++] = short(sym);
  return left;
}
bool codes(State& s, const Huff& lencode, const Huff& distcode) {
  static const short lens[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
  static const short lext[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
  static const int dists[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073,
                                4097, 6145, 8193, 12289, 16385, 24577};
  static const short dext[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
  for (;;) {
    int sym = decode(s, lencode);
    if (sym < 0 || s.err) return false;
    if (sym < 256) { s.out.append(char(sym)); continue; }
    if (sym == 256) return true;
    sym -= 257;
    if (sym >= 29) return false;
    const int len = lens[sym] + bits(s, lext[sym]);
    const int ds = decode(s, distcode);
    if (ds < 0 || ds >= 30) return false;
    const int dist = dists[ds] + bits(s, dext[ds]);
    if (s.err || dist > s.out.size()) return false;
    qsizetype from = s.out.size() - dist;
    for (int k = 0; k < len; ++k) s.out.append(s.out.at(from + k));
  }
}
}

QByteArray inflateRaw(const QByteArray& data, bool* ok) {
  State s; s.in = reinterpret_cast<const uchar*>(data.constData()); s.len = data.size();
  s.out.reserve(data.size() * 4);
  bool good = true;
  int last = 0;
  do {
    last = bits(s, 1);
    const int type = bits(s, 2);
    if (s.err) { good = false; break; }
    if (type == 0) {
      s.bitbuf = 0; s.bitcnt = 0;
      if (s.pos + 4 > s.len) { good = false; break; }
      const int len = s.in[s.pos] | (s.in[s.pos + 1] << 8);
      s.pos += 4;
      if (s.pos + len > s.len) { good = false; break; }
      s.out.append(reinterpret_cast<const char*>(s.in + s.pos), len);
      s.pos += len;
    } else if (type == 1) {
      Huff lc, dc;
      {
        short l[320];
        int i = 0;
        for (; i < 144; ++i) l[i] = 8;
        for (; i < 256; ++i) l[i] = 9;
        for (; i < 280; ++i) l[i] = 7;
        for (; i < 288; ++i) l[i] = 8;
        construct(lc, l, 288);
        for (i = 0; i < 30; ++i) l[i] = 5;
        construct(dc, l, 30);
      }
      if (!codes(s, lc, dc)) { good = false; break; }
    } else if (type == 2) {
      static const short order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
      const int nlen = bits(s, 5) + 257, ndist = bits(s, 5) + 1, ncode = bits(s, 4) + 4;
      if (s.err || nlen > 286 || ndist > 30) { good = false; break; }
      short lengths[320] = {0};
      for (int i = 0; i < ncode; ++i) lengths[order[i]] = short(bits(s, 3));
      Huff lc, dc;
      if (construct(lc, lengths, 19) != 0) { good = false; break; }
      int index = 0;
      while (index < nlen + ndist && good) {
        int sym = decode(s, lc);
        if (sym < 0) { good = false; break; }
        if (sym < 16) { lengths[index++] = short(sym); continue; }
        int len = 0;
        if (sym == 16) { if (index == 0) { good = false; break; } len = lengths[index - 1]; sym = 3 + bits(s, 2); }
        else if (sym == 17) sym = 3 + bits(s, 3);
        else sym = 11 + bits(s, 7);
        if (index + sym > nlen + ndist) { good = false; break; }
        while (sym--) lengths[index++] = short(len);
      }
      if (!good || s.err || lengths[256] == 0) { good = false; break; }
      const int e1 = construct(lc, lengths, nlen);
      if (e1 < 0 || (e1 > 0 && nlen - lc.count[0] != 1)) { good = false; break; }
      const int e2 = construct(dc, lengths + nlen, ndist);
      if (e2 < 0 || (e2 > 0 && ndist - dc.count[0] != 1)) { good = false; break; }
      if (!codes(s, lc, dc)) { good = false; break; }
    } else { good = false; break; }
  } while (!last);
  if (ok) *ok = good && !s.err;
  return good ? s.out : QByteArray();
}

static quint16 u16(const QByteArray& b, qsizetype o) { return o + 2 <= b.size() ? qFromLittleEndian<quint16>(b.constData() + o) : 0; }
static quint32 u32(const QByteArray& b, qsizetype o) { return o + 4 <= b.size() ? qFromLittleEndian<quint32>(b.constData() + o) : 0; }

QHash<QString, QByteArray> unzip(const QByteArray& z, const QStringList& only) {
  QHash<QString, QByteArray> out;
  qsizetype eocd = -1;
  for (qsizetype i = z.size() - 22; i >= 0 && i >= z.size() - 70000; --i)
    if (u32(z, i) == 0x06054b50) { eocd = i; break; }
  if (eocd < 0) return out;
  const int n = u16(z, eocd + 10);
  qsizetype p = u32(z, eocd + 16);
  for (int k = 0; k < n && p + 46 <= z.size(); ++k) {
    if (u32(z, p) != 0x02014b50) break;
    const int method = u16(z, p + 10);
    const quint32 csize = u32(z, p + 20);
    const int nl = u16(z, p + 28), el = u16(z, p + 30), cl = u16(z, p + 32);
    const quint32 loc = u32(z, p + 42);
    const QString name = QString::fromUtf8(z.mid(p + 46, nl));
    p += 46 + nl + el + cl;
    if (!only.isEmpty() && !only.contains(name)) continue;
    if (u32(z, loc) != 0x04034b50) continue;
    const qsizetype data = loc + 30 + u16(z, loc + 26) + u16(z, loc + 28);
    const QByteArray raw = z.mid(data, csize);
    if (method == 0) out.insert(name, raw);
    else if (method == 8) { bool ok = false; QByteArray d = inflateRaw(raw, &ok); if (ok) out.insert(name, d); }
  }
  return out;
}

static int colIndex(const QString& ref) {
  int c = 0;
  for (QChar ch : ref) { if (!ch.isLetter()) break; c = c * 26 + (ch.toUpper().unicode() - 'A' + 1); }
  return c - 1;
}

QList<QList<QVariant>> sheet(const QByteArray& file, const QString& sheetName, bool* ok) {
  QList<QList<QVariant>> rows;
  if (ok) *ok = false;
  const auto files = unzip(file);
  // sheet name → rId → target
  QString rid;
  {
    QXmlStreamReader x(files.value("xl/workbook.xml"));
    while (!x.atEnd()) {
      if (x.readNext() == QXmlStreamReader::StartElement && x.name() == u"sheet" &&
          x.attributes().value("name").toString() == sheetName) {
        for (const auto& a : x.attributes()) if (a.qualifiedName().endsWith(u":id")) rid = a.value().toString();
      }
    }
  }
  if (rid.isEmpty()) return rows;
  QString target;
  {
    QXmlStreamReader x(files.value("xl/_rels/workbook.xml.rels"));
    while (!x.atEnd())
      if (x.readNext() == QXmlStreamReader::StartElement && x.name() == u"Relationship" && x.attributes().value("Id") == rid)
        target = x.attributes().value("Target").toString();
  }
  if (target.isEmpty()) return rows;
  if (target.startsWith('/')) target = target.mid(1); else target = "xl/" + target;
  QStringList shared;
  {
    QXmlStreamReader x(files.value("xl/sharedStrings.xml"));
    QString cur; bool inSi = false;
    while (!x.atEnd()) {
      const auto t = x.readNext();
      if (t == QXmlStreamReader::StartElement && x.name() == u"si") { inSi = true; cur.clear(); }
      else if (t == QXmlStreamReader::StartElement && x.name() == u"t" && inSi) cur += x.readElementText();
      else if (t == QXmlStreamReader::EndElement && x.name() == u"si") { shared << cur; inSi = false; }
    }
  }
  const QByteArray xml = files.value(target);
  if (xml.isEmpty()) return rows;
  QXmlStreamReader x(xml);
  QList<QVariant> row; int rowIdx = -1, nextCol = 0;
  QString ref, type; QString v; bool inC = false;
  while (!x.atEnd()) {
    const auto t = x.readNext();
    if (t == QXmlStreamReader::StartElement) {
      if (x.name() == u"row") {
        // `r` is optional (ECMA-376): without it the row follows the previous one
        int r = x.attributes().value("r").toInt() - 1;
        if (r < 0) r = rowIdx + 1;
        while (rows.size() < r) rows << QList<QVariant>{};
        row.clear(); rowIdx = r; nextCol = 0;
      } else if (x.name() == u"c") {
        inC = true; ref = x.attributes().value("r").toString(); type = x.attributes().value("t").toString(); v.clear();
      } else if (inC && (x.name() == u"v" || x.name() == u"t")) v = x.readElementText();
    } else if (t == QXmlStreamReader::EndElement) {
      if (x.name() == u"c") {
        inC = false;
        int c = colIndex(ref);
        if (c < 0) c = nextCol;   // a cell without `r` follows the previous cell
        nextCol = c + 1;
        while (row.size() <= c) row << QVariant();
        if (type == "s") row[c] = shared.value(v.toInt());
        else if (type == "str" || type == "inlineStr") row[c] = v;
        else if (type == "e") row[c] = QVariant();
        else { bool okd = false; const double d = v.toDouble(&okd); row[c] = okd ? QVariant(d) : QVariant(); }
      } else if (x.name() == u"row") {
        if (rowIdx >= 0) { while (rows.size() <= rowIdx) rows << QList<QVariant>{}; rows[rowIdx] = row; }
      }
    }
  }
  if (ok) *ok = !x.hasError() || !rows.isEmpty();
  return rows;
}
}
