"""Deepwater ML trainer (AI › ML). Run by the app inside a macOS sandbox:
    python train.py <run folder> [<data set>]
It reads <run>/job.json and the data set (the app's own file, read-only, or <run>/data.csv), writes <run>/result.json, <run>/predictions.csv, <run>/model.joblib,
and prints one JSON object per line on stdout: {"event": "progress", "stage": ..., "pct": ...}, {"event": "log", ...},
{"event": "done"} or {"event": "error", "message": ...}.

No network, no file outside the run folder and the Python environment: the sandbox refuses them.
Time series care: rows are kept in date order, the split is chronological (no shuffling), the target looks forward
`horizon` rows and is never used as a feature at the same row.
"""
import json
import math
import os
import sys
import time
import traceback
import warnings

warnings.filterwarnings("ignore")
ARGS = [a for a in sys.argv[1:] if not a.startswith("--")]
FLAGS = {a for a in sys.argv[1:] if a.startswith("--")}   # --refs: add the references to a finished run (refs_only)
RUN = ARGS[0] if len(ARGS) > 0 else "."
# the app's data set read in place (the sandbox grants this one file, read-only) rather than a copy per run
DATA = ARGS[1] if len(ARGS) > 1 else os.path.join(RUN, "data.csv")
# filled by frame(): what the run learned from (result "data"), and the persistence reference of the target (see frame)
DATA_INFO = {}
PERSIST = None
# filled by frame() when the target is a price (all values above 0): per row the price, the return to the next row and
# a trend rule known at the row (price above its 20-row average) — what trading_eval() turns predictions into money with
TRADE = None
# filled by frame() for the volatility targets: per row, what the HAR and EWMA references read (known at the row)
VOLREF = None
VOL_TARGETS = ("realized_vol", "range", "abs_return", "big_move")
T0 = time.time()


def emit(**kw):
    print(json.dumps(kw, default=float), flush=True)


def progress(stage, pct):
    emit(event="progress", stage=stage, pct=round(pct, 1))


def point(curve, x, **values):
    # one point of a curve drawn live by the app (AI › Agents' activity panel): the score of a fold, the loss of an epoch,
    # the return of an episode; values = one number per line of the curve (None = no value at this point)
    emit(event="point", curve=curve, x=x, values={k: (None if v is None else float(v)) for k, v in values.items()})


def clean(v):
    """JSON-safe numbers (NaN/inf → None)."""
    if isinstance(v, float):
        return v if math.isfinite(v) else None
    if isinstance(v, dict):
        return {k: clean(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [clean(x) for x in v]
    try:
        import numpy as np
        if isinstance(v, np.generic):
            return clean(v.item())
    except Exception:
        pass
    return v


MLP_MAX_LAYERS, MLP_MAX_UNITS = 6, 1024
# Methods whose time or memory grows with the square of the rows: the latest rows only (the test part stays whole)
SUPERVISED_ROW_CAP = {"svm": 20000, "knn": 200000}
UNSUPERVISED_ROW_CAP = {"agglomerative": 8000, "tsne": 5000, "dbscan": 50000, "lof": 200000}


def load_job():
    with open(os.path.join(RUN, "job.json"), encoding="utf-8") as f:
        return json.load(f)


# ---------------------------------------------------------------------------------------------------- data
def frame(job, pd, np, target_required=True):
    """Every row in date order: features X (with lags), the target/outcome y looking `horizon` rows ahead (None when
    there is none), dates, feature names, transform."""
    df = pd.read_csv(DATA)
    date_col = job.get("date_column") or ("date" if "date" in df.columns else None)
    if date_col and date_col in df.columns:
        df[date_col] = pd.to_datetime(df[date_col], errors="coerce", utc=True)
        # an unparsable date sorts last: it became the end of the test part and the "latest row" predicted now
        bad = int(df[date_col].isna().sum())
        if bad:
            emit(event="log", text=f"{bad} row(s) with a date that cannot be read were left out")
            df = df[df[date_col].notna()]
        df = df.sort_values(date_col).reset_index(drop=True)
    global PERSIST
    DATA_INFO.update({"rows": int(len(df)),
                      "first": str(df[date_col].iloc[0]) if date_col and len(df) else None,
                      "last": str(df[date_col].iloc[-1]) if date_col and len(df) else None})
    target = job.get("target") or ""
    transform = job.get("target_transform", "raw")
    h = int(job.get("horizon", 0))
    y = None
    persist = None
    if not target:
        if target_required:
            raise ValueError("no target column")
    elif target not in df.columns:
        raise ValueError(f"the column '{target}' is not in the data")
    elif transform != "raw" and h < 1:
        raise ValueError("a change, direction or volatility target needs a horizon of at least 1 row")
    global VOLREF
    VOLREF = None
    y_src = pd.to_numeric(df[target], errors="coerce") if target else None
    if y_src is None:
        pass
    elif transform == "raw":
        y = y_src.shift(-h) if h > 0 else y_src
        persist = y_src if h > 0 else y_src.shift(1)
    elif transform == "change":
        y = y_src.shift(-h) - y_src
        persist = y_src - y_src.shift(h)
    elif transform == "pct_change":
        y = (y_src.shift(-h) / y_src - 1.0) * 100.0
        persist = (y_src / y_src.shift(h) - 1.0) * 100.0
    elif transform == "direction":
        d = y_src.shift(-h) - y_src
        y = pd.Series(np.where(d.isna(), np.nan, np.where(d > 0, 1.0, 0.0)), index=df.index)
        d0 = y_src - y_src.shift(h)
        persist = pd.Series(np.where(d0.isna(), np.nan, np.where(d0 > 0, 1.0, 0.0)), index=df.index)
    elif transform in VOL_TARGETS:
        # step 5 of the roadmap (2026-11-20): how much the price will move, not where. Over the next h rows (t+1 … t+h):
        #   realized_vol  √(Σ squared log returns) in %          range       (highest high − lowest low) / price now, %
        #   abs_return    |price in h rows / price now − 1|, %     big_move    1 when the furthest excursion from the
        #                 price now reaches move_pct (default: the 80th percentile of the training part's excursions)
        # persistence: the same measure over the last h rows. HAR and EWMA references: see vol_refs().
        if not (float((y_src > 0).mean()) > 0.99):
            raise ValueError(f"{transform} needs a price column (every value above 0)")
        lr = np.log(y_src).diff()
        hi = pd.to_numeric(df["high"], errors="coerce") if "high" in df.columns else y_src
        lo = pd.to_numeric(df["low"], errors="coerce") if "low" in df.columns else y_src
        def ahead(s, how):   # the next h rows' sum / max / min, at each row (NaN where they are not all known)
            r = getattr(s[::-1].rolling(h, min_periods=h), how)()[::-1]
            return r.shift(-1)
        def behind(s, how):
            return getattr(s.rolling(h, min_periods=h), how)()
        if transform == "realized_vol":
            y = np.sqrt(ahead(lr ** 2, "sum")) * 100.0
            persist = np.sqrt(behind(lr ** 2, "sum")) * 100.0
        elif transform == "range":
            y = (ahead(hi, "max") - ahead(lo, "min")) / y_src * 100.0
            persist = (behind(hi, "max") - behind(lo, "min")) / y_src * 100.0
        elif transform == "abs_return":
            y = (y_src.shift(-h) / y_src - 1.0).abs() * 100.0
            persist = (y_src / y_src.shift(h) - 1.0).abs() * 100.0
        else:
            exc = np.maximum(ahead(hi, "max") / y_src - 1.0, 1.0 - ahead(lo, "min") / y_src) * 100.0
            past = np.maximum(behind(hi, "max") / y_src.shift(h) - 1.0, 1.0 - behind(lo, "min") / y_src.shift(h)) * 100.0
            thr = float(job.get("move_pct", 0) or 0)
            if thr <= 0:   # from the training part only (the rows before the test share), never from the test rows
                cut0 = int(round(len(exc) * (1 - min(max(float(job.get("test_size", 0.2)), 0.05), 0.5))))
                thr = float(np.nanquantile(exc.iloc[:max(1, cut0 - h)].to_numpy(dtype=float), 0.8))   # whose windows end before the test
            DATA_INFO["move_threshold_pct"] = thr
            y = pd.Series(np.where(exc.isna(), np.nan, (exc >= thr).astype(float)), index=df.index)
            persist = pd.Series(np.where(past.isna(), np.nan, (past >= thr).astype(float)), index=df.index)
        VOLREF = vol_ref_columns(lr, h, pd, np)
    else:
        raise ValueError(f"unknown target transform '{transform}'")
    # persistence: "what was last seen happens again" — the target's own value measured up to this row (the last change
    # over the same horizon, the last direction, the level now), known at the row: a naive rule harder to beat than the
    # training mean. Same rows as y.
    if persist is not None:
        persist = persist.replace([np.inf, -np.inf], np.nan)
    if y is not None:
        # a zero in the target column gives ±inf with pct_change (or an "inf" in the file): scikit-learn stops on it
        inf = np.isinf(y.to_numpy(dtype=float))
        if inf.any():
            emit(event="log", text=f"{int(inf.sum())} row(s) with an infinite target (a zero in '{target}'?) were left out")
            keep = ~inf
            df, y = df[keep].reset_index(drop=True), y[keep].reset_index(drop=True)
            y_src = y_src[keep].reset_index(drop=True)
            if VOLREF is not None:
                VOLREF = VOLREF[keep].reset_index(drop=True)
            if persist is not None:
                persist = persist[keep].reset_index(drop=True)
    PERSIST = persist
    global TRADE
    TRADE = None
    if y_src is not None and len(y_src) and float((y_src > 0).mean()) > 0.99:
        sma = y_src.rolling(20, min_periods=20).mean()
        TRADE = pd.DataFrame({"price": y_src.to_numpy(dtype=float), "next": (y_src.shift(-1) / y_src - 1.0).to_numpy(dtype=float),
                              "trend": np.where(sma.isna(), np.nan, (y_src > sma).astype(float))}, index=df.index)
        TRADE = TRADE.replace([np.inf, -np.inf], np.nan)

    feats = [c for c in job.get("features", []) if c in df.columns and c != date_col]
    if target_required and h == 0 and target in feats:
        feats.remove(target)          # the answer itself, at the same row
    if not feats:
        raise ValueError("no feature column selected")
    X = df[feats].apply(pd.to_numeric, errors="coerce")
    xinf = np.isinf(X.to_numpy(dtype=float)).any(axis=1)
    if xinf.any():   # an infinite feature: the row is unusable, as a missing value
        emit(event="log", text=f"{int(xinf.sum())} row(s) with an infinite feature value were left out")
        X = X.replace([np.inf, -np.inf], np.nan)
    lags = int(job.get("lags", 0))
    for k in range(1, lags + 1):
        for c in feats:
            X[f"{c}_lag{k}"] = X[c].shift(k)
    dates = df[date_col] if date_col and date_col in df.columns else pd.Series(range(len(df)), index=df.index)
    return X, y, dates, list(X.columns), transform


def prepare(job, pd, np):
    global PERSIST
    X, y, dates, names, transform = frame(job, pd, np)
    # rows usable for training: every feature and the target known
    ok = X.notna().all(axis=1) & y.notna()
    PERSIST = PERSIST[ok] if PERSIST is not None else None
    global TRADE, VOLREF
    TRADE = TRADE[ok] if TRADE is not None else None
    VOLREF = VOLREF[ok] if VOLREF is not None else None
    # the most recent rows whose future is not known yet: what the model predicts "now"
    latest_mask = X.notna().all(axis=1) & y.isna()
    return X[ok], y[ok], dates[ok], X[latest_mask], dates[latest_mask], names, transform


# ---------------------------------------------------------------------------------------------------- models
def make_model(job, task, n_features):
    from sklearn.pipeline import make_pipeline
    from sklearn.preprocessing import StandardScaler
    algo = job.get("algorithm", "random_forest")
    p = job.get("params", {})
    seed = int(job.get("seed", 42))
    cls = task == "classification"
    scale = False
    if algo == "linear":
        from sklearn.linear_model import LogisticRegression, Ridge
        c = float(p.get("regularization", 1.0))
        m = LogisticRegression(C=1.0 / max(c, 1e-6), max_iter=2000) if cls else Ridge(alpha=c)
        scale = True
    elif algo == "random_forest":
        from sklearn.ensemble import RandomForestClassifier, RandomForestRegressor
        kw = dict(n_estimators=int(p.get("n_estimators", 300)), max_depth=(int(p["max_depth"]) or None) if p.get("max_depth") else None,
                  min_samples_leaf=int(p.get("min_samples_leaf", 5)), random_state=seed, n_jobs=1)
        m = RandomForestClassifier(**kw) if cls else RandomForestRegressor(**kw)
    elif algo == "hist_gb":
        from sklearn.ensemble import HistGradientBoostingClassifier, HistGradientBoostingRegressor
        kw = dict(max_iter=int(p.get("n_estimators", 300)), learning_rate=float(p.get("learning_rate", 0.05)),
                  max_depth=(int(p["max_depth"]) or None) if p.get("max_depth") else None, random_state=seed)
        m = HistGradientBoostingClassifier(**kw) if cls else HistGradientBoostingRegressor(**kw)
    elif algo == "lightgbm":
        import lightgbm as lgb
        kw = dict(n_estimators=int(p.get("n_estimators", 300)), learning_rate=float(p.get("learning_rate", 0.05)),
                  max_depth=int(p.get("max_depth", 0)) or -1, random_state=seed, n_jobs=1, verbose=-1)
        m = lgb.LGBMClassifier(**kw) if cls else lgb.LGBMRegressor(**kw)
    elif algo == "xgboost":
        import xgboost as xgb
        kw = dict(n_estimators=int(p.get("n_estimators", 300)), learning_rate=float(p.get("learning_rate", 0.05)),
                  max_depth=int(p.get("max_depth", 0)) or 6, random_state=seed, n_jobs=1)
        m = xgb.XGBClassifier(**kw) if cls else xgb.XGBRegressor(**kw)
    elif algo == "knn":
        from sklearn.neighbors import KNeighborsClassifier, KNeighborsRegressor
        k = int(p.get("n_neighbors", 15))
        m = KNeighborsClassifier(n_neighbors=k) if cls else KNeighborsRegressor(n_neighbors=k)
        scale = True
    elif algo == "svm":
        from sklearn.svm import SVC, SVR
        c = float(p.get("c", 1.0))
        m = SVC(C=c, probability=True, random_state=seed) if cls else SVR(C=c)
        scale = True
    elif algo == "mlp":
        from sklearn.neural_network import MLPClassifier, MLPRegressor
        sizes = tuple(int(s) for s in str(p.get("hidden_layers", "64,32")).replace(" ", "").split(",") if s)
        # bounded: "9999,9999,9999" is 200 million weights (gigabytes) and no sandbox limit on memory
        bounded = tuple(min(max(s, 1), MLP_MAX_UNITS) for s in sizes[:MLP_MAX_LAYERS])
        if bounded != sizes:
            emit(event="log", text=f"hidden layers {','.join(map(str, sizes))} brought to {','.join(map(str, bounded))} "
                                   f"(at most {MLP_MAX_LAYERS} layers of {MLP_MAX_UNITS})")
        sizes = bounded
        kw = dict(hidden_layer_sizes=sizes or (64, 32), max_iter=int(p.get("max_iter", 500)), early_stopping=True, random_state=seed)
        m = MLPClassifier(**kw) if cls else MLPRegressor(**kw)
        scale = True
    else:
        raise ValueError(f"unknown algorithm '{algo}'")
    return make_pipeline(StandardScaler(), m) if scale else m


def importances(model, X_test, y_test, names, task):
    import numpy as np
    est = model.steps[-1][1] if hasattr(model, "steps") else model
    vals, how = None, ""
    if hasattr(est, "feature_importances_"):
        vals, how = np.asarray(est.feature_importances_, dtype=float), "model"
    elif hasattr(est, "coef_"):
        vals, how = np.abs(np.asarray(est.coef_, dtype=float)).reshape(-1, len(names)).mean(axis=0), "coefficients"
    if vals is None or len(vals) != len(names):
        from sklearn.inspection import permutation_importance
        r = permutation_importance(model, X_test, y_test, n_repeats=5, random_state=0,
                                   scoring="balanced_accuracy" if task == "classification" else "neg_mean_absolute_error")
        vals, how = np.maximum(r.importances_mean, 0), "permutation"
    s = float(vals.sum()) or 1.0
    pairs = sorted(zip(names, (vals / s).tolist()), key=lambda t: -t[1])[:30]
    return {"method": how, "items": [{"feature": n, "share": v} for n, v in pairs]}


def scores(task, y_true, y_pred, proba=None, transform="raw"):
    import numpy as np
    from sklearn import metrics as M
    if task == "classification":
        out = {"accuracy": M.accuracy_score(y_true, y_pred),
               "balanced_accuracy": M.balanced_accuracy_score(y_true, y_pred),
               "precision": M.precision_score(y_true, y_pred, average="macro", zero_division=0),
               "recall": M.recall_score(y_true, y_pred, average="macro", zero_division=0),
               "f1": M.f1_score(y_true, y_pred, average="macro", zero_division=0)}
        if proba is not None and len(np.unique(y_true)) == 2:
            try:
                out["roc_auc"] = M.roc_auc_score(y_true, proba)
            except ValueError:
                pass
        return out
    out = {"mae": M.mean_absolute_error(y_true, y_pred),
           "rmse": math.sqrt(M.mean_squared_error(y_true, y_pred)),
           "r2": M.r2_score(y_true, y_pred)}
    if transform in ("change", "pct_change"):
        nz = np.asarray(y_true) != 0
        if nz.any():
            out["direction_accuracy"] = float(np.mean(np.sign(np.asarray(y_pred)[nz]) == np.sign(np.asarray(y_true)[nz])))
    return out


def reference_columns(out, task, transform, base, pers_te, np):
    """The two references written next to each test row (ml_compare reads them; nothing has to be rebuilt):
    no_skill = the baseline's constant (majority class or training mean), persistence = the target's last seen value
    (empty where it is not known). Returns the persistence's scores on the rows where it is known."""
    const = base.get("class") if task == "classification" else base.get("value")
    out["no_skill"] = const
    pers = np.asarray(pers_te, dtype=float)
    if task == "classification":
        pers = np.where(np.isnan(pers), np.nan, np.round(pers))
    out["persistence"] = pers
    ok = ~np.isnan(pers)
    if ok.sum() < 10:
        return None
    y = out["actual"].to_numpy()[ok]
    p = pers[ok].astype(int) if task == "classification" else pers[ok]
    return {"rule": "persistence", "rows": int(ok.sum()), **scores(task, y.astype(int) if task == "classification" else y, p, transform=transform)}


def supervised(job):
    import numpy as np
    import pandas as pd
    progress("load", 5)
    X, y, dates, X_latest, dates_latest, names, transform = prepare(job, pd, np)
    task = job.get("task", "auto")
    if task == "auto":
        task = "classification" if transform == "direction" or (y.nunique() <= 10 and np.allclose(y, y.round())) else "regression"
    n = len(X)
    if n < 40:
        raise ValueError(f"only {n} usable rows after removing missing values: too few to train and test")
    test_size = min(max(float(job.get("test_size", 0.2)), 0.05), 0.5)
    cut = int(round(n * (1 - test_size)))
    # the target looks h rows ahead: the last h training rows' targets are computed from test-period prices, so they
    # are left out of training (and the cross-validation folds keep the same gap)
    h = max(0, int(job.get("horizon", 0)))
    tr_end = cut - h
    if tr_end < 20:
        raise ValueError(f"only {tr_end} training rows once the {h} rows before the test part are left out: too few")
    X_tr, X_te, y_tr, y_te = X.iloc[:tr_end], X.iloc[cut:], y.iloc[:tr_end], y.iloc[cut:]
    cap = SUPERVISED_ROW_CAP.get(job.get("algorithm", ""))
    if cap and len(X_tr) > cap:   # a kernel SVM on a million rows does not end; neighbours grow with the rows too
        X_tr, y_tr = X_tr.iloc[-cap:], y_tr.iloc[-cap:]
        emit(event="log", text=f"{job.get('algorithm')}: trained on the latest {cap} training rows")
    if task == "classification":
        y_tr, y_te = y_tr.astype(int), y_te.astype(int)
        if y_tr.nunique() < 2:
            raise ValueError("the training part holds a single class")
    emit(event="log", text=f"{n} rows, {len(names)} features, train {len(X_tr)} / test {len(X_te)}, task {task}")
    progress("prepare", 12)

    # walk-forward (job "validation": "walk_forward", 2026-11-20): the test part is cut into `wf_folds` blocks in time
    # order; each block is predicted by a model trained afresh on every row before it, minus the h rows whose target
    # reaches into the block (purge), as a model retrained regularly would have been used. The test rows are the same as
    # with one cut: the two kinds of runs are compared on the same rows.
    validation = job.get("validation", "split")
    wf = None
    cv = {}
    if validation == "walk_forward":
        k = min(max(int(job.get("wf_folds", 5)), 2), 20)
        n_te = n - cut
        if n_te < k * 5:
            raise ValueError(f"walk-forward: {n_te} test rows for {k} blocks, at least 5 rows a block are needed")
        edges = [cut + (n_te * j) // k for j in range(k + 1)]
        pred = np.empty(n_te, dtype=float)
        proba = np.full(n_te, np.nan) if task == "classification" else None
        nos = np.empty(n_te, dtype=object if task == "classification" else float)
        blocks = []
        metric = "balanced_accuracy" if task == "classification" else "mae"
        for j in range(k):
            s0, s1 = edges[j], edges[j + 1]
            te_end = s0 - h
            if te_end < 20:
                raise ValueError(f"walk-forward: only {te_end} training rows before block {j + 1}: too few")
            Xa, ya = X.iloc[:te_end], y.iloc[:te_end]
            if cap and len(Xa) > cap:
                Xa, ya = Xa.iloc[-cap:], ya.iloc[-cap:]
            if task == "classification":
                ya = ya.astype(int)
                if ya.nunique() < 2:
                    raise ValueError(f"walk-forward: the training rows before block {j + 1} hold a single class")
            m = make_model(job, task, len(names))
            m.fit(Xa, ya)
            pb = m.predict(X.iloc[s0:s1])
            pred[s0 - cut:s1 - cut] = pb
            if proba is not None and hasattr(m, "predict_proba") and ya.nunique() == 2:
                proba[s0 - cut:s1 - cut] = m.predict_proba(X.iloc[s0:s1])[:, 1]
            # no skill as it would have been known then: the majority class or the mean of these training rows
            nos[s0 - cut:s1 - cut] = int(ya.mode().iloc[0]) if task == "classification" else float(ya.mean())
            yb = y.iloc[s0:s1].astype(int) if task == "classification" else y.iloc[s0:s1]
            sc = scores(task, yb, pb, transform=transform)[metric]
            blocks.append({"from": str(dates.iloc[s0]), "to": str(dates.iloc[s1 - 1]), "rows": int(s1 - s0),
                           "train_rows": int(len(Xa)), metric: sc})
            point("wf_" + metric, j + 1, block=sc)
            progress(f"walk-forward {j + 1}/{k}", 12 + 70 * (j + 1) / k)
            model, X_tr, y_tr = m, Xa, ya
        if task == "classification":
            pred = pred.astype(int)
            if np.isnan(proba).all():
                proba = None
        wf = {"mode": "walk_forward", "folds": k, "gap_rows": h, "metric": metric, "blocks": blocks,
              "anchored": True, "note": "each block predicted by a model trained on every row before it, the h rows "
                                        "whose target reaches into the block left out"}
        fit_s = 0.0
        progress("evaluate", 85)
    else:
        # time-series cross-validation on the training part
        folds = int(job.get("cv_folds", 0))
        if folds >= 2:
            from sklearn.model_selection import TimeSeriesSplit
            vals = []
            for i, (a, b) in enumerate(TimeSeriesSplit(n_splits=folds, gap=h).split(X_tr)):
                m = make_model(job, task, len(names))
                m.fit(X_tr.iloc[a], y_tr.iloc[a])
                pr = m.predict(X_tr.iloc[b])
                s = scores(task, y_tr.iloc[b], pr, transform=transform)
                vals.append(s["balanced_accuracy"] if task == "classification" else s["mae"])
                point("cv_balanced_accuracy" if task == "classification" else "cv_mae", i + 1, fold=vals[-1])
                progress(f"cv {i + 1}/{folds}", 12 + 48 * (i + 1) / folds)
            cv = {"metric": "balanced_accuracy" if task == "classification" else "mae", "folds": folds,
                  "mean": float(np.mean(vals)), "std": float(np.std(vals)), "values": vals}

        progress("fit", 62)
        model = make_model(job, task, len(names))
        t = time.time()
        model.fit(X_tr, y_tr)
        fit_s = time.time() - t
        progress("evaluate", 85)
        pred = model.predict(X_te)
        proba = None
        if task == "classification" and hasattr(model, "predict_proba") and y_tr.nunique() == 2:
            proba = model.predict_proba(X_te)[:, 1]
        nos = None
    test = scores(task, y_te, pred, proba, transform)
    train = scores(task, y_tr, model.predict(X_tr), transform=transform)

    # baselines: what a model with no skill gets on the same test rows (walk-forward: each block's own, known then)
    if task == "classification":
        majority = int(y_tr.mode().iloc[0])
        base = {"rule": "majority", "class": majority,
                **scores(task, y_te, np.full(len(y_te), majority) if nos is None else nos.astype(int), transform=transform)}
        from sklearn.metrics import confusion_matrix
        labels = sorted(set(y_tr.unique()) | set(y_te.unique()))
        cm = {"labels": [int(v) for v in labels], "matrix": confusion_matrix(y_te, pred, labels=labels).tolist()}
    else:
        mean = float(y_tr.mean())
        base = {"rule": "mean_direction" if transform in ("change", "pct_change") else "mean", "value": mean,
                **scores(task, y_te, np.full(len(y_te), mean) if nos is None else nos.astype(float), transform=transform)}
        cm = None
    if nos is not None:
        base["per_block"] = True   # the rule's value is each block's own; "class" / "value" = the last block's

    imp = importances(model, X_te, y_te, names, task)
    progress("save", 95)
    import joblib
    joblib.dump({"model": model, "features": names, "task": task, "job": job}, os.path.join(RUN, "model.joblib"))
    out = pd.DataFrame({"date": dates.iloc[cut:].astype(str).values, "actual": y_te.values, "predicted": pred})
    if proba is not None:
        out["probability"] = proba
    pers_te = PERSIST.iloc[cut:].to_numpy(dtype=float) if PERSIST is not None else np.full(len(y_te), np.nan)
    persistence = reference_columns(out, task, transform, base, pers_te, np)
    if nos is not None:
        out["no_skill"] = nos
    out.to_csv(os.path.join(RUN, "predictions.csv"), index=False)

    latest = None
    if len(X_latest):
        lp = model.predict(X_latest.iloc[[-1]])[0]
        latest = {"date": str(dates_latest.iloc[-1]), "prediction": float(lp)}
        if task == "classification" and hasattr(model, "predict_proba") and y_tr.nunique() == 2:
            latest["probability"] = float(model.predict_proba(X_latest.iloc[[-1]])[0, 1])

    result = {"family": "supervised", "task": task, "rows": n, "train_rows": len(X_tr), "test_rows": len(X_te),
              "features": names, "train_period": [str(dates.iloc[0]), str(dates.iloc[tr_end - 1])],
              "test_period": [str(dates.iloc[cut]), str(dates.iloc[-1])], "test": test, "train": train,
              "baseline": base, "persistence": persistence, "cv": cv, "gap_rows": h, "confusion": cm, "importances": imp, "latest": latest,
              "fit_seconds": fit_s, "seconds": time.time() - T0, "versions": versions()}
    if VOLREF is not None:   # volatility targets: the HAR and EWMA references, fitted on the same training rows as the model
        segments = [(edges[j] - h, edges[j], edges[j + 1]) for j in range(len(edges) - 1)] if wf else [(tr_end, cut, n)]
        rp = vol_refs(task, VOLREF, y, segments, np)
        block = max(10, 2 * h)
        refs = {}
        for name, rule in (("har", "HAR: a regression on the realized volatility of the last h, 5h and 22h rows (Corsi)"),
                           ("ewma", "EWMA: a regression on the exponentially weighted volatility of the row returns (λ 0.94)")):
            pr = rp[name].astype(int) if task == "classification" else rp[name]
            refs[name] = {"rule": rule, **scores(task, y_te, pr, transform=transform)}
            refs["vs_" + name] = paired_p(task, y_te.to_numpy(dtype=float), pred, pr, block, np)
            out[name] = pr
        out.to_csv(os.path.join(RUN, "predictions.csv"), index=False)
        result["references"] = refs
    if "move_threshold_pct" in DATA_INFO:
        result["move_threshold_pct"] = DATA_INFO["move_threshold_pct"]
    trade_te = TRADE.iloc[cut:] if TRADE is not None else None
    tr = trading_eval(job, task, transform, pred, trade_te, dates.iloc[cut:], np)
    if tr:
        result["trading"] = tr
    if wf:
        result["walk_forward"] = wf
        result["train_period"] = [str(dates.iloc[0]), str(dates.iloc[edges[-2] - h - 1])]
    with open(os.path.join(RUN, "result.json"), "w", encoding="utf-8") as f:
        json.dump(clean(result), f, indent=1)
    progress("done", 100)


def vol_ref_columns(lr, h, pd, np):
    """The two references a volatility forecast must beat (arXiv 1802.04065: an EWMA beats most models on BTC): known at
    each row, scaled to the next h rows. HAR (Corsi 2009): the realized volatility of the last h, 5h and 22h rows, each per
    h rows; EWMA (RiskMetrics, λ 0.94): √(h × exponentially weighted variance of the row returns). Both are turned into
    the target's own unit by a regression fitted on the training rows only (vol_refs)."""
    sq = lr ** 2
    cols = {}
    for name, k in (("har_d", 1), ("har_w", 5), ("har_m", 22)):
        n = h * k
        cols[name] = np.sqrt(sq.rolling(n, min_periods=n).mean() * h) * 100.0
    var = sq.ewm(alpha=0.06, adjust=False, min_periods=20).mean()
    cols["ewma"] = np.sqrt(var * h) * 100.0
    return pd.DataFrame(cols, index=lr.index).replace([np.inf, -np.inf], np.nan)


def vol_refs(task, ref, y, segments, np):
    """HAR and EWMA predictions of the test rows: for each segment (train_end, s0, s1) a linear (or, for big_move, a
    logistic) regression fitted on the rows before train_end, predicting rows s0..s1. Rows without the inputs get the
    training mean (or majority). Returns {"har": array, "ewma": array} over the test rows (positions segments[0][1]…)."""
    from sklearn.linear_model import LinearRegression, LogisticRegression
    out = {}
    for name, cols in (("har", ["har_d", "har_w", "har_m"]), ("ewma", ["ewma"])):
        parts = []
        for tr_end, s0, s1 in segments:
            Xa = ref[cols].iloc[:tr_end].to_numpy(dtype=float)
            ya = y.iloc[:tr_end].to_numpy(dtype=float)
            ok = np.isfinite(Xa).all(axis=1) & np.isfinite(ya)
            Xb = ref[cols].iloc[s0:s1].to_numpy(dtype=float)
            fill = (float(np.round(np.nanmean(ya)) if task == "classification" else np.nanmean(ya)))
            pb = np.full(s1 - s0, fill)
            okb = np.isfinite(Xb).all(axis=1)
            if ok.sum() >= 30 and (task != "classification" or len(np.unique(ya[ok])) == 2):
                # balanced classes: judged on balanced accuracy, a reference that always says "no big move" would be a straw man
                m = LogisticRegression(max_iter=1000, class_weight="balanced") if task == "classification" else LinearRegression()
                m.fit(Xa[ok], ya[ok].astype(int) if task == "classification" else ya[ok])
                if okb.any():
                    pb[okb] = m.predict(Xb[okb])
            parts.append(pb)
        out[name] = np.concatenate(parts)
    return out


def paired_p(task, actual, a, b, block, np):
    """Is A better than B on the same rows? Per-row score (classification: right, weighted so the mean is the balanced
    accuracy; values: minus the absolute error), moving-block bootstrap of the mean difference A − B: p = share of
    resamples at or below 0 (small: A better), p_worse = share at or above 0 (small: A worse)."""
    act = np.asarray(actual, dtype=float)
    def rows(p):
        p = np.asarray(p, dtype=float)
        if task != "classification":
            return -np.abs(p - act)
        cls, cnt = np.unique(act, return_counts=True)
        w = {c: len(act) / (len(cls) * n) for c, n in zip(cls, cnt)}
        return np.where(np.round(p) == act, np.vectorize(w.get)(act), 0.0)
    d = rows(a) - rows(b)
    L = len(d)
    rng = np.random.default_rng(11)
    nb = int(np.ceil(L / block))
    means = np.empty(2000)
    for i in range(2000):
        st = rng.integers(0, max(1, L - block + 1), nb)
        means[i] = d[np.minimum((st[:, None] + np.arange(block)).ravel()[:L], L - 1)].mean()
    return {"diff": float(d.mean()), "p": float(np.mean(means <= 0)), "p_worse": float(np.mean(means >= 0))}


def trading_eval(job, task, transform, pred, trade_te, dates_te, np):
    """Step 4 of the roadmap (2026-11-20): the test predictions as a position held from each row to the next (decided at
    the row's close: long when the model says up, else flat — or short with trade_side long_short), paying cost_bp a
    side (job, set by the app: Binance 0.10 % fee + 0.05 % slippage as the Strategy tester, less for forex), against
    buy & hold, a trend rule (above its 20-row average) and a constant exposure equal to the model's average (timing
    skill apart from simply being in the market). None when the target is not a price or the classes are not up / down."""
    if trade_te is None or len(trade_te) < 20 or transform in VOL_TARGETS:   # a volatility forecast is not a direction to trade
        return None
    if task == "classification" and transform != "direction":
        return None
    r = trade_te["next"].to_numpy(dtype=float)
    r = np.where(np.isfinite(r), r, 0.0)
    price = trade_te["price"].to_numpy(dtype=float)
    p = np.asarray(pred, dtype=float)
    up = p >= 0.5 if task == "classification" else (p > price if transform == "raw" else p > 0)
    side = job.get("trade_side", "long")
    low = -1.0 if side == "long_short" else 0.0
    pos = np.where(up, 1.0, low)
    cost = max(0.0, float(job.get("cost_bp", 15.0))) / 1e4
    try:   # rows per year from the test dates; 252 when the file has no dates
        span = (pd_ts(dates_te.iloc[-1]) - pd_ts(dates_te.iloc[0])).total_seconds() / (365.25 * 86400) if hasattr(dates_te.iloc[0], "year") else 0
    except Exception:
        span = 0
    ppy = (len(r) - 1) / span if span > 0.01 else 252.0

    def sret_of(ps, c):
        ch = np.abs(np.diff(np.concatenate([[0.0], ps])))
        return ps * r - c * ch

    model = {f"x{k}": perf(pos, r, 0, cost * k, ppy, np)[0] for k in (0, 1, 2, 3)}
    trend = trade_te["trend"].to_numpy(dtype=float)
    tpos = np.where(np.isnan(trend), 0.0, np.where(trend > 0, 1.0, low))
    expo = float(np.mean(pos))
    cpos = np.full(len(r), expo)
    # does the timing earn more than the same average exposure held all along? paired, after costs, moving blocks
    d = sret_of(pos, cost) - sret_of(cpos, cost)
    block = max(10, 2 * int(job.get("horizon", 1)))
    rng = np.random.default_rng(7)
    L = len(d)
    nb = int(np.ceil(L / block))
    means = np.empty(2000)
    for i in range(2000):
        st = rng.integers(0, max(1, L - block + 1), nb)
        idx = (st[:, None] + np.arange(block)).ravel()[:L]
        means[i] = d[np.minimum(idx, L - 1)].mean()
    return {"rule": ("long when the model says up, short otherwise" if side == "long_short" else "long when the model says up, flat otherwise")
                    + "; the position is decided at each test row's close and held to the next row",
            "side": side, "cost_bp": cost * 1e4, "rows": int(L), "rows_per_year": ppy,
            "model": model, "buy_hold": perf(np.ones(L), r, 0, cost, ppy, np)[0],
            "trend": {"rule": "long when the price is above its 20-row average" + (", short below" if side == "long_short" else ", flat below"),
                      **perf(tpos, r, 0, cost, ppy, np)[0]},
            "constant": {**perf(cpos, r, 0, cost, ppy, np)[0], "level": expo},
            "vs_constant": {"mean_diff_bp": float(d.mean() * 1e4), "p": float(np.mean(means <= 0)), "p_worse": float(np.mean(means >= 0)),
                            "block": block,
                            "note": "p: share of 2000 moving-block resamples where the model's return per row, after costs, is not above "
                                    "the constant exposure's; p_worse: not below it"}}


def pd_ts(v):
    import pandas as pd
    return pd.Timestamp(v)


# ---------------------------------------------------------------------------------------------------- unsupervised
CLUSTERING = ("kmeans", "gmm", "agglomerative", "dbscan")
ANOMALY = ("isolation_forest", "lof")
REDUCTION = ("pca", "tsne")


def group_profiles(labels, Z, Xraw, y, dates, names, transform, np):
    """One entry per group: size, period, how long it lasts in a row, what sets it apart (mean z-scores), and, when an
    outcome column is given, what followed it."""
    out = []
    n = len(labels)
    for lab in sorted(set(labels.tolist()), key=lambda v: (v < 0, v)):
        idx = np.where(labels == lab)[0]
        z = Z[idx].mean(axis=0)
        order = np.argsort(-np.abs(z))[:6]
        # stretches: consecutive rows in the same group (regime persistence)
        runs, prev = 0, None
        for v in labels:
            if v == lab and prev != lab:
                runs += 1
            prev = v
        g = {"label": int(lab), "size": int(len(idx)), "share": len(idx) / n,
             "first": str(dates.iloc[idx[0]]), "last": str(dates.iloc[idx[-1]]),
             "avg_stretch": len(idx) / max(runs, 1),
             "defining": [{"feature": names[k], "z": float(z[k]), "mean": float(Xraw[idx, k].mean())} for k in order]}
        if y is not None:
            yy = y[idx]
            yy = yy[~np.isnan(yy)]
            if len(yy):
                g["outcome"] = {"n": int(len(yy)), "mean": float(yy.mean()), "median": float(np.median(yy)),
                                "up_share": float((yy > 0).mean()) if transform in ("change", "pct_change", "direction") else None}
        out.append(g)
    return out


def unsupervised(job):
    import numpy as np
    import pandas as pd
    from sklearn.preprocessing import StandardScaler
    from sklearn.decomposition import PCA
    progress("load", 5)
    X, y, dates, names, transform = frame(job, pd, np, target_required=False)
    ok = X.notna().all(axis=1)
    X, dates = X[ok].reset_index(drop=True), dates[ok].reset_index(drop=True)
    yv = y[ok].to_numpy(dtype=float) if y is not None else None
    method = job.get("algorithm", "kmeans")
    p = job.get("params", {})
    seed = int(job.get("seed", 42))
    limit = UNSUPERVISED_ROW_CAP.get(method)
    if limit and len(X) > limit:   # these methods grow with the square of the rows: keep the latest ones
        X, dates = X.iloc[-limit:].reset_index(drop=True), dates.iloc[-limit:].reset_index(drop=True)
        yv = yv[-limit:] if yv is not None else None
        emit(event="log", text=f"{method}: kept the latest {limit} rows")
    n = len(X)
    if n < 20:
        raise ValueError(f"only {n} usable rows after removing missing values: too few")
    Xraw = X.to_numpy(dtype=float)
    Z = StandardScaler().fit_transform(Xraw)
    # extreme values (bad ticks, crashes) would otherwise get a group or an axis of their own: clipped at ±clip σ
    clip = float(job.get("clip_sigma", 5))
    clipped = 0
    Zfull = Z.copy()                                     # unclipped, to explain unusual rows with their real size
    if clip > 0:
        clipped = int((np.abs(Z) > clip).sum())
        Z = np.clip(Z, -clip, clip)
    emit(event="log", text=f"{n} rows, {len(names)} features, method {method}, {clipped} values clipped")
    progress("prepare", 15)

    res = {"family": "unsupervised", "method": method, "rows": n, "features": names, "clip_sigma": clip, "clipped": clipped,
           "period": [str(dates.iloc[0]), str(dates.iloc[-1])]}
    if job.get("target"):
        res["outcome"] = {"column": job["target"], "transform": transform, "horizon": int(job.get("horizon", 0))}
    labels = np.zeros(n, dtype=int)
    score = np.zeros(n)
    xy = PCA(n_components=2, random_state=0).fit_transform(Z) if Z.shape[1] >= 2 else np.c_[Z[:, 0], np.zeros(n)]
    model = None
    progress("fit", 30)

    if method in CLUSTERING:
        res["kind"] = "clustering"
        k = int(p.get("n_clusters", 4))
        if method == "kmeans":
            from sklearn.cluster import KMeans
            model = KMeans(n_clusters=k, n_init=10, random_state=seed).fit(Z)
            labels = model.labels_
            score = model.transform(Z).min(axis=1)        # distance to its group's centre
            scan = int(p.get("scan_max", 0))
            if scan >= 3:
                from sklearn.metrics import silhouette_score
                res["k_scan"] = []
                for i, kk in enumerate(range(2, scan + 1)):
                    m = KMeans(n_clusters=kk, n_init=5, random_state=seed).fit(Z)
                    res["k_scan"].append({"k": kk, "inertia": float(m.inertia_),
                                          "silhouette": float(silhouette_score(Z, m.labels_, sample_size=min(n, 3000), random_state=0))})
                    progress(f"k {kk}", 30 + 40 * (i + 1) / (scan - 1))
        elif method == "gmm":
            from sklearn.mixture import GaussianMixture
            model = GaussianMixture(n_components=k, covariance_type=str(p.get("covariance", "full")), random_state=seed, n_init=3).fit(Z)
            labels = model.predict(Z)
            score = model.predict_proba(Z).max(axis=1)    # how sure the row's group is
            res["bic"], res["aic"] = float(model.bic(Z)), float(model.aic(Z))
        elif method == "agglomerative":
            from sklearn.cluster import AgglomerativeClustering
            model = AgglomerativeClustering(n_clusters=k, linkage=str(p.get("linkage", "ward"))).fit(Z)
            labels = model.labels_
        else:
            from sklearn.cluster import DBSCAN
            model = DBSCAN(eps=float(p.get("eps", 1.5)), min_samples=int(p.get("min_samples", 10))).fit(Z)
            labels = model.labels_
        # groups numbered by size (0 = largest); DBSCAN's noise stays -1
        sizes = sorted({int(v) for v in labels if v >= 0}, key=lambda v: -(labels == v).sum())
        remap = {old: new for new, old in enumerate(sizes)}
        labels = np.array([remap.get(int(v), -1) for v in labels])
        progress("measure", 75)
        real = labels >= 0
        q = {}
        if len(set(labels[real].tolist())) >= 2:
            from sklearn import metrics as M
            q["silhouette"] = float(M.silhouette_score(Z[real], labels[real], sample_size=min(int(real.sum()), 3000), random_state=0))
            q["davies_bouldin"] = float(M.davies_bouldin_score(Z[real], labels[real]))
            q["calinski_harabasz"] = float(M.calinski_harabasz_score(Z[real], labels[real]))
        res["quality"] = q
        res["groups"] = group_profiles(labels, Z, Xraw, yv, dates, names, transform, np)
        res["noise_share"] = float((labels < 0).mean())
        res["latest"] = {"date": str(dates.iloc[-1]), "label": int(labels[-1])}

    elif method in ANOMALY:
        res["kind"] = "anomaly"
        cont = min(max(float(p.get("contamination", 0.02)), 0.001), 0.5)
        if method == "isolation_forest":
            from sklearn.ensemble import IsolationForest
            model = IsolationForest(n_estimators=int(p.get("n_estimators", 300)), contamination=cont, random_state=seed).fit(Z)
            score = -model.score_samples(Z)
            flag = model.predict(Z) == -1
        else:
            from sklearn.neighbors import LocalOutlierFactor
            model = LocalOutlierFactor(n_neighbors=int(p.get("n_neighbors", 20)), contamination=cont)
            flag = model.fit_predict(Z) == -1
            score = -model.negative_outlier_factor_
        labels = flag.astype(int)                          # 1 = unusual
        progress("measure", 75)
        top = np.argsort(-score)[:25]
        res["anomalies"] = [{"date": str(dates.iloc[i]), "score": float(score[i]),
                             "why": [{"feature": names[k], "z": float(Zfull[i, k]), "value": float(Xraw[i, k])}
                                     for k in np.argsort(-np.abs(Zfull[i]))[:3]]} for i in top]
        res["groups"] = group_profiles(labels, Z, Xraw, yv, dates, names, transform, np)
        res["anomaly_share"] = float(flag.mean())
        rank = float((score < score[-1]).mean())
        res["latest"] = {"date": str(dates.iloc[-1]), "label": int(labels[-1]), "score": float(score[-1]), "rank": rank}

    elif method in REDUCTION:
        res["kind"] = "reduction"
        if method == "pca":
            k = max(1, min(int(p.get("n_components", 3)), Z.shape[1]))
            model = PCA(n_components=k, random_state=0).fit(Z)
            comp = model.transform(Z)
            xy = comp[:, :2] if k >= 2 else np.c_[comp[:, 0], np.zeros(n)]
            res["components"] = [{"explained": float(model.explained_variance_ratio_[i]),
                                  "top": [{"feature": names[j], "loading": float(model.components_[i, j])}
                                          for j in np.argsort(-np.abs(model.components_[i]))[:5]]} for i in range(k)]
        else:
            from sklearn.manifold import TSNE
            perp = min(float(p.get("perplexity", 30)), (n - 1) / 3)
            xy = TSNE(n_components=2, perplexity=perp, init="pca", random_state=seed).fit_transform(Z)
            model = None
        progress("measure", 75)
    else:
        raise ValueError(f"unknown method '{method}'")

    progress("save", 90)
    out = pd.DataFrame({"date": dates.astype(str).values, "label": labels, "score": score, "x": xy[:, 0], "y": xy[:, 1]})
    if yv is not None:
        out["outcome"] = yv
    out.to_csv(os.path.join(RUN, "assignments.csv"), index=False)
    if model is not None:
        import joblib
        joblib.dump({"model": model, "features": names, "job": job}, os.path.join(RUN, "model.joblib"))
    res["seconds"] = time.time() - T0
    res["versions"] = versions()
    with open(os.path.join(RUN, "result.json"), "w", encoding="utf-8") as f:
        json.dump(clean(res), f, indent=1)
    progress("done", 100)


# ---------------------------------------------------------------------------------------------------- semi-supervised
def semi_base(name, seed):
    from sklearn.pipeline import make_pipeline
    from sklearn.preprocessing import StandardScaler
    if name == "gradient_boosting":
        from sklearn.ensemble import HistGradientBoostingClassifier
        return HistGradientBoostingClassifier(max_iter=200, learning_rate=0.05, random_state=seed)
    if name == "random_forest":
        from sklearn.ensemble import RandomForestClassifier
        return RandomForestClassifier(n_estimators=300, min_samples_leaf=5, random_state=seed, n_jobs=1)
    from sklearn.linear_model import LogisticRegression
    return make_pipeline(StandardScaler(), LogisticRegression(max_iter=2000))


def semi_split(job, pd, np):
    """semi_supervised()'s rows, split and kept labels, as a dict: shared with refs_only(), which must rebuild exactly the
    same test rows and no-skill class as the run did."""
    X, y, dates, names, transform = frame(job, pd, np)
    persist_all = PERSIST
    xok = X.notna().all(axis=1)
    lab_idx = np.where(xok & y.notna())[0]
    if len(lab_idx) < 40:
        raise ValueError("too few labelled rows")
    last = lab_idx[-1]
    rows = np.where(xok.to_numpy() & (np.arange(len(X)) <= last))[0]          # labelled + blank-target rows
    latest_rows = np.where(xok.to_numpy() & (np.arange(len(X)) > last))[0]     # future unknown: predicted "now"
    Xr, yr, dr = X.iloc[rows].reset_index(drop=True), y.iloc[rows].reset_index(drop=True), dates.iloc[rows].reset_index(drop=True)
    vals = yr.dropna()
    if not (transform == "direction" or (vals.nunique() <= 10 and np.allclose(vals, vals.round()))):
        raise ValueError("semi-supervised learning works on classes here: pick the direction, or a column of classes")
    # scikit-learn's semi-supervised models read the class -1 as "no label": a real class -1 (down / flat / up as
    # -1/0/1) lost every one of its labels. The classes are coded 0..k-1 for the models and decoded for the results.
    classes = np.sort(vals.astype(int).unique())
    code = {int(c): i for i, c in enumerate(classes)}
    yr = yr.map(lambda v: code[int(v)] if pd.notna(v) else np.nan)

    def back(a):
        return classes[np.asarray(a, dtype=int)]
    n = len(Xr)
    test_size = min(max(float(job.get("test_size", 0.2)), 0.05), 0.5)
    cut = int(round(n * (1 - test_size)))
    # as in supervised(): the target looks h rows ahead, so the labels of the last h training rows are computed from
    # test-period prices. Those rows stay in training unlabelled (their features only look back), their labels hidden.
    h = max(0, int(job.get("horizon", 0)))
    tr_end = cut - h
    if tr_end < 20:
        raise ValueError(f"only {tr_end} training rows once the {h} rows before the test part are left out: too few")
    tr_lab = np.where(yr.iloc[:tr_end].notna())[0]
    gap = int(yr.iloc[tr_end:cut].notna().sum())
    natural = int(yr.iloc[:cut].isna().sum())
    te = np.where(yr.iloc[cut:].notna())[0] + cut
    p = job.get("params", {})
    seed = int(job.get("seed", 42))
    share = min(max(float(p.get("labelled_share", 0.1)), 0.005), 1.0)
    rng = np.random.default_rng(seed)
    keep = np.sort(rng.choice(tr_lab, size=max(10, int(round(len(tr_lab) * share))), replace=False)) if share < 1 else tr_lab
    y_semi = np.full(cut, -1, dtype=int)
    y_semi[keep] = yr.iloc[keep].astype(int).to_numpy()
    if len(set(y_semi[keep].tolist())) < 2:
        raise ValueError("the kept labels hold a single class: raise the labelled share")
    return {"X": X, "y": y, "dates": dates, "names": names, "transform": transform, "persist": persist_all, "rows": rows,
            "latest_rows": latest_rows, "Xr": Xr, "yr": yr, "dr": dr, "classes": classes, "back": back, "n": n, "cut": cut,
            "h": h, "tr_end": tr_end, "tr_lab": tr_lab, "gap": gap, "natural": natural, "te": te, "p": p, "seed": seed,
            "share": share, "keep": keep, "y_semi": y_semi}


def semi_supervised(job):
    """Few labels, many unlabelled rows. The training part keeps the labels of `labelled_share` of its rows (the others
    are hidden), plus any row whose target is blank in the file; the model learns from both. Compared on the same test
    rows with the same model trained on the kept labels only, and with every label (the upper bound)."""
    import numpy as np
    import pandas as pd
    from sklearn.preprocessing import StandardScaler
    progress("load", 5)
    S = semi_split(job, pd, np)
    X, y, dates, names, transform, persist_all = S["X"], S["y"], S["dates"], S["names"], S["transform"], S["persist"]
    rows, latest_rows, Xr, yr, dr, classes, back = S["rows"], S["latest_rows"], S["Xr"], S["yr"], S["dr"], S["classes"], S["back"]
    n, cut, h, tr_end, tr_lab, gap, natural, te, p, seed, share, keep, y_semi = (S[k] for k in (
        "n", "cut", "h", "tr_end", "tr_lab", "gap", "natural", "te", "p", "seed", "share", "keep", "y_semi"))
    emit(event="log", text=f"{n} rows: train {cut} ({len(keep)} labelled, {cut - len(keep)} unlabelled"
                           f"{f', {gap} labels hidden for the {h}-row horizon' if gap else ''}), test {len(te)}")
    progress("prepare", 12)

    method = job.get("algorithm", "self_training")
    base_name = str(p.get("base", "gradient_boosting"))
    X_tr, X_te, y_te = Xr.iloc[:cut], Xr.iloc[te], yr.iloc[te].astype(int)
    progress("fit", 25)
    extra = {}
    if method == "self_training":
        from sklearn.semi_supervised import SelfTrainingClassifier
        model = SelfTrainingClassifier(semi_base(base_name, seed), threshold=float(p.get("threshold", 0.8)),
                                       max_iter=int(p.get("max_iter", 10)))
        model.fit(X_tr, y_semi)
        extra["pseudo_labelled"] = int((model.labeled_iter_ > 0).sum())
        extra["iterations"] = int(model.n_iter_)
        labels_only = semi_base(base_name, seed)
        full = semi_base(base_name, seed)
    else:
        from sklearn.pipeline import make_pipeline
        from sklearn.neighbors import KNeighborsClassifier
        from sklearn.semi_supervised import LabelSpreading, LabelPropagation
        k = int(p.get("n_neighbors", 10))
        core = LabelSpreading(kernel="knn", n_neighbors=k, alpha=float(p.get("alpha", 0.2)), max_iter=100) if method == "label_spreading" \
            else LabelPropagation(kernel="knn", n_neighbors=k, max_iter=1000)
        model = make_pipeline(StandardScaler(), core).fit(X_tr, y_semi)
        labels_only = make_pipeline(StandardScaler(), KNeighborsClassifier(n_neighbors=min(k, len(keep))))
        full = make_pipeline(StandardScaler(), KNeighborsClassifier(n_neighbors=k))
    progress("compare", 60)
    labels_only.fit(X_tr.iloc[keep], y_semi[keep])
    tr_y_all = yr.iloc[tr_lab].astype(int)
    full.fit(X_tr.iloc[tr_lab], tr_y_all)
    progress("evaluate", 80)

    def proba_of(m, Xs):
        try:
            return m.predict_proba(Xs)[:, 1] if len(m.classes_) == 2 else None
        except Exception:
            return None
    pred = back(model.predict(X_te))     # the user's classes from here on
    y_te = pd.Series(back(y_te), index=y_te.index)
    proba = proba_of(model, X_te)
    test = scores("classification", y_te, pred, proba)
    train = scores("classification", back(yr.iloc[keep]), back(model.predict(X_tr.iloc[keep])))
    comp = {"labels_only": scores("classification", y_te, back(labels_only.predict(X_te)), proba_of(labels_only, X_te)),
            "all_labels": scores("classification", y_te, back(full.predict(X_te)), proba_of(full, X_te))}
    majority = int(back([pd.Series(y_semi[keep]).mode().iloc[0]])[0])
    base = {"rule": "majority", "class": majority, **scores("classification", y_te, np.full(len(y_te), majority))}
    from sklearn.metrics import confusion_matrix
    labels = sorted(set(int(v) for v in y_te.unique()) | set(int(v) for v in back(y_semi[keep])))
    cm = {"labels": [int(v) for v in labels], "matrix": confusion_matrix(y_te, pred, labels=labels).tolist()}
    progress("save", 92)
    import joblib
    joblib.dump({"model": model, "features": names, "job": job}, os.path.join(RUN, "model.joblib"))
    out = pd.DataFrame({"date": dr.iloc[te].astype(str).values, "actual": y_te.values, "predicted": pred})
    if proba is not None:
        out["probability"] = proba
    pr_all = persist_all.iloc[rows].reset_index(drop=True) if persist_all is not None else None
    persistence = reference_columns(out, "classification", transform, base,
                                    pr_all.iloc[te].to_numpy(dtype=float) if pr_all is not None else np.full(len(te), np.nan), np)
    out.to_csv(os.path.join(RUN, "predictions.csv"), index=False)
    # the test predictions traded after costs, as for supervised runs (2026-11-20): the test rows are rows[te] of the frame
    trading = trading_eval(job, "classification", transform, pred, TRADE.iloc[rows[te]], dr.iloc[te], np) if TRADE is not None else None
    latest = None
    if len(latest_rows):
        xl = X.iloc[[latest_rows[-1]]]
        latest = {"date": str(dates.iloc[latest_rows[-1]]), "prediction": float(back(model.predict(xl))[0])}
        pl = proba_of(model, xl)
        if pl is not None:
            latest["probability"] = float(pl[0])
    result = {"family": "semi_supervised", "task": "classification", "rows": n, "train_rows": cut, "test_rows": len(te),
              "features": names, "train_period": [str(dr.iloc[0]), str(dr.iloc[cut - 1])],
              "test_period": [str(dr.iloc[cut]), str(dr.iloc[-1])], "test": test, "train": train, "baseline": base,
              "persistence": persistence, "cv": {}, "confusion": cm, "importances": {}, "latest": latest,
              "semi": {"labelled": int(len(keep)), "hidden": int(len(tr_lab) - len(keep)), "blank": natural,
                       "horizon_gap": gap,
                       "share": share, **comp, **extra},
              "seconds": time.time() - T0, "versions": versions()}
    if trading:
        result["trading"] = trading
    with open(os.path.join(RUN, "result.json"), "w", encoding="utf-8") as f:
        json.dump(clean(result), f, indent=1)
    progress("done", 100)


# ---------------------------------------------------------------------------------------------------- self-supervised
def self_supervised(job):
    """Learns a representation of windows of `window` rows from the data alone (no target), with PyTorch on the CPU:
      masked_autoencoder  rebuild values hidden at random in the window
      contrastive         two noisy copies of a window must land close together, other windows apart (SimCLR)
      next_step           predict the next row from the window
    Trained on the training part only; then every window gets an embedding (a short vector). Reused here to draw a map,
    to rank unusual windows (rebuild / prediction error), and, when a probe target is given, to compare a simple
    linear model on the embeddings with the same model on the raw features, on the later test rows."""
    import numpy as np
    import pandas as pd
    import torch
    import torch.nn as nn
    torch.set_num_threads(4)
    progress("load", 3)
    X, y, dates, names, transform = frame(job, pd, np, target_required=False)
    ok = X.notna().all(axis=1).to_numpy()
    first = int(np.argmax(ok))
    X, dates = X.iloc[first:].reset_index(drop=True), dates.iloc[first:].reset_index(drop=True)
    y = y.iloc[first:].reset_index(drop=True) if y is not None else None
    persist_all = PERSIST.iloc[first:].reset_index(drop=True) if PERSIST is not None else None
    trade_all = TRADE.iloc[first:].reset_index(drop=True) if TRADE is not None else None
    X = X.ffill().bfill()   # the odd gap inside the history
    p = job.get("params", {})
    seed = int(job.get("seed", 42))
    torch.manual_seed(seed)
    np.random.seed(seed)
    W = int(p.get("window", 20))
    emb_dim = int(p.get("embedding", 16))
    epochs = int(p.get("epochs", 40))
    lr = float(p.get("learning_rate", 0.001))
    method = job.get("algorithm", "masked_autoencoder")
    n, F = X.shape
    if n < W + 50:
        raise ValueError(f"only {n} rows: too few for windows of {W}")
    test_size = min(max(float(job.get("test_size", 0.2)), 0.05), 0.5)
    cut = int(round(n * (1 - test_size)))
    mu, sd = X.iloc[:cut].mean(), X.iloc[:cut].std().replace(0, 1)          # scaled with the training part only
    Z = np.ascontiguousarray(np.clip(((X - mu) / sd).to_numpy(dtype=np.float32), -5, 5))
    ends = np.arange(W - 1, n)
    # windows × W × F as a view of Z (window i = rows i … i+W-1, ending at ends[i]): np.stack copied every window, W
    # times the data (14 GB for windows of 120 on a million rows); batches are copied as they are used
    win = np.lib.stride_tricks.sliding_window_view(Z, W, axis=0).transpose(0, 2, 1)
    nxt = np.vstack([Z[ends[:-1] + 1], np.zeros((1, F), dtype=np.float32)])     # the row after each window
    train_w = ends < cut - (1 if method == "next_step" else 0)
    valid_w = (ends >= cut) & (ends < n - (1 if method == "next_step" else 0))
    emit(event="log", text=f"{len(ends)} windows of {W} rows × {F} features; pre-training on {int(train_w.sum())}, method {method}")
    progress("prepare", 8)

    class Encoder(nn.Module):
        def __init__(self):
            super().__init__()
            self.gru = nn.GRU(F, 64, batch_first=True)
            self.head = nn.Linear(64, emb_dim)
        def forward(self, x):
            _, h = self.gru(x)
            return self.head(h[-1])
    enc = Encoder()
    if method == "masked_autoencoder":
        dec = nn.Sequential(nn.Linear(emb_dim, 128), nn.ReLU(), nn.Linear(128, W * F))
    elif method == "next_step":
        dec = nn.Sequential(nn.Linear(emb_dim, 64), nn.ReLU(), nn.Linear(64, F))
    elif method == "contrastive":
        dec = nn.Sequential(nn.Linear(emb_dim, 64), nn.ReLU(), nn.Linear(64, 32))
    else:
        raise ValueError(f"unknown method '{method}'")
    params = list(enc.parameters()) + list(dec.parameters())
    opt = torch.optim.Adam(params, lr=lr)
    ratio = float(p.get("mask_ratio", 0.3))
    noise = float(p.get("noise", 0.1))
    temp = float(p.get("temperature", 0.2))

    def augment(x):
        x = x * (1 + noise * torch.randn(x.shape[0], 1, x.shape[2])) + noise * torch.randn_like(x)
        drop = torch.rand(x.shape[0], x.shape[1], 1) < 0.1
        return x.masked_fill(drop, 0.0)

    def loss_on(xb, nb, per_row=False):
        if method == "masked_autoencoder":
            mask = torch.rand_like(xb) < ratio
            rec = dec(enc(xb.masked_fill(mask, 0.0))).view_as(xb)
            err = ((rec - xb) ** 2 * mask).sum(dim=(1, 2)) / mask.sum(dim=(1, 2)).clamp(min=1)
        elif method == "next_step":
            err = ((dec(enc(xb)) - nb) ** 2).mean(dim=1)
        else:
            a = nn.functional.normalize(dec(enc(augment(xb))), dim=1)
            b = nn.functional.normalize(dec(enc(augment(xb))), dim=1)
            z = torch.cat([a, b])
            sim = z @ z.T / temp
            sim.fill_diagonal_(-1e9)
            m = xb.shape[0]
            target = torch.cat([torch.arange(m, 2 * m), torch.arange(0, m)])
            err = nn.functional.cross_entropy(sim, target, reduction="none")
        return err if per_row else err.mean()

    # the same view for PyTorch (from_numpy refuses a read-only array): no copy
    Wt = torch.from_numpy(Z).as_strided((len(ends), W, F), (F, F, 1))
    Nt = torch.from_numpy(nxt)
    tr_idx, va_idx = np.where(train_w)[0], np.where(valid_w)[0]
    history = []
    bs = 256
    for ep in range(epochs):
        enc.train(); dec.train()
        perm = np.random.permutation(tr_idx)
        tot, cnt = 0.0, 0
        for i in range(0, len(perm), bs):
            b = perm[i:i + bs]
            if len(b) < 2:
                continue
            loss = loss_on(Wt[b], Nt[b])
            opt.zero_grad(); loss.backward(); opt.step()
            tot += float(loss) * len(b); cnt += len(b)
        enc.eval(); dec.eval()
        with torch.no_grad():
            vl = float(np.mean([float(loss_on(Wt[va_idx[i:i + bs]], Nt[va_idx[i:i + bs]])) for i in range(0, len(va_idx), bs)])) if len(va_idx) > 1 else None
        history.append({"epoch": ep + 1, "train": tot / max(cnt, 1), "valid": vl})
        point("loss", ep + 1, train=tot / max(cnt, 1), valid=vl)
        emit(event="progress", stage=f"epoch {ep + 1}/{epochs} · loss {tot / max(cnt, 1):.4f}", pct=round(10 + 70 * (ep + 1) / epochs, 1))

    progress("embed", 82)
    enc.eval(); dec.eval()
    with torch.no_grad():
        E = torch.cat([enc(Wt[i:i + 1024]) for i in range(0, len(Wt), 1024)]).numpy()
        torch.manual_seed(seed)
        if method == "contrastive":
            score = np.zeros(len(E))
        else:
            score = torch.cat([loss_on(Wt[i:i + 1024], Nt[i:i + 1024], per_row=True) for i in range(0, len(Wt), 1024)]).numpy()
            if method == "next_step":
                score[-1] = np.nan
    from sklearn.decomposition import PCA
    xy = PCA(n_components=2, random_state=0).fit_transform(E)
    wdates = dates.iloc[ends].reset_index(drop=True)
    out = pd.DataFrame({"date": wdates.astype(str).values, "label": 0, "score": np.nan_to_num(score), "x": xy[:, 0], "y": xy[:, 1]})
    for k in range(E.shape[1]):
        out[f"emb_{k}"] = E[:, k]
    out.to_csv(os.path.join(RUN, "assignments.csv"), index=False)

    res = {"family": "self_supervised", "method": method, "rows": n, "windows": int(len(ends)), "window": W,
           "embedding": emb_dim, "features": names, "period": [str(dates.iloc[0]), str(dates.iloc[-1])],
           "pretrain_period": [str(dates.iloc[W - 1]), str(dates.iloc[cut - 1])], "history": history}
    if method != "contrastive":
        valid = ~np.isnan(score)
        top = np.argsort(-np.where(valid, score, -np.inf))[:15]
        res["unusual"] = [{"date": str(wdates.iloc[i]), "error": float(score[i])} for i in top]
        res["error_median"] = float(np.nanmedian(score))

    # probe: does the representation carry what a target needs? (simple linear model, later rows as test)
    if y is not None and job.get("target"):
        progress("probe", 88)
        h = int(job.get("horizon", 0))
        yw = y.iloc[ends].to_numpy(dtype=float)
        cls = transform == "direction"
        tr = (ends + h < cut) & ~np.isnan(yw)
        te = (ends >= cut) & ~np.isnan(yw)
        if tr.sum() >= 30 and te.sum() >= 10:
            from sklearn.linear_model import LogisticRegression, Ridge
            from sklearn.pipeline import make_pipeline
            from sklearn.preprocessing import StandardScaler
            task = "classification" if cls else "regression"
            probe_pred = {}
            def fit_eval(F_all, tr=tr, te=te, yw=yw, keep=False):
                m = make_pipeline(StandardScaler(), LogisticRegression(max_iter=2000) if cls else Ridge(alpha=1.0))
                yt = yw[tr].astype(int) if cls else yw[tr]
                ye = yw[te].astype(int) if cls else yw[te]
                m.fit(F_all[tr], yt)
                pr = m.predict(F_all[te])
                if keep:
                    probe_pred["predicted"] = pr
                proba = m.predict_proba(F_all[te])[:, 1] if cls and len(m.classes_) == 2 else None
                return scores(task, ye, pr, proba, transform)
            raw_now = Z[ends]
            # the flat windows are a copy of W × F values per window: past ~200 million values, windows evenly spread
            # over the whole period (training and later windows both) instead of all of them
            keep = min(len(ends), max(1, int(2e8 // (W * F))))
            sel = np.arange(len(ends)) if keep == len(ends) else np.unique(np.linspace(0, len(ends) - 1, keep).astype(int))
            if len(sel) < len(ends):
                emit(event="log", text=f"probe on raw windows: {len(sel)} of {len(ends)} windows, evenly spread (memory)")
            raw_win = win[sel].reshape(len(sel), -1)
            raw_ok = tr[sel].sum() >= 30 and te[sel].sum() >= 10
            ye = yw[te].astype(int) if cls else yw[te]
            if cls:
                maj = int(pd.Series(yw[tr].astype(int)).mode().iloc[0])
                nos = scores(task, ye, np.full(len(ye), maj), transform=transform)
            else:
                nos = scores(task, ye, np.full(len(ye), float(np.mean(yw[tr]))), transform=transform)
            res["probe"] = {"task": task, "target": job["target"], "transform": transform, "horizon": h,
                            "train_windows": int(tr.sum()), "test_windows": int(te.sum()),
                            "embeddings": fit_eval(E, keep=True), "raw_now": fit_eval(raw_now),
                            "raw_window": fit_eval(raw_win, tr[sel], te[sel], yw[sel]) if raw_ok else None,
                            "no_skill": nos}
            # the probe's test windows, with both references next to them, for ml_compare
            nos_base = {"class": maj} if cls else {"value": float(np.mean(yw[tr]))}
            pout = pd.DataFrame({"date": wdates[te].astype(str).values, "actual": ye, "predicted": probe_pred["predicted"]})
            pw = persist_all.iloc[ends].to_numpy(dtype=float)[te] if persist_all is not None else np.full(int(te.sum()), np.nan)
            res["probe"]["persistence"] = reference_columns(pout, task, transform, nos_base, pw, np)
            res["probe"]["no_skill"]["value" if not cls else "class"] = nos_base.get("value", nos_base.get("class"))
            pout.to_csv(os.path.join(RUN, "probe_predictions.csv"), index=False)
            # the probe's test predictions traded after costs: each test window decides at its last row (2026-11-20)
            if trade_all is not None:
                tr_eval = trading_eval(job, task, transform, probe_pred["predicted"], trade_all.iloc[ends[te]], wdates[te], np)
                if tr_eval:
                    res["probe"]["trading"] = tr_eval
    progress("save", 95)
    torch.save({"encoder": enc.state_dict(), "features": names, "window": W, "embedding": emb_dim,
                "mean": mu.tolist(), "std": sd.tolist(), "method": method}, os.path.join(RUN, "encoder.pt"))
    res["seconds"] = time.time() - T0
    res["versions"] = {**versions(), "torch": torch.__version__}
    with open(os.path.join(RUN, "result.json"), "w", encoding="utf-8") as f:
        json.dump(clean(res), f, indent=1)
    progress("done", 100)


# ---------------------------------------------------------------------------------------------------- reinforcement
def perf(pos, r, prev0, cost, ppy, np):
    """Simulated result of holding pos[t] over each step t → t+1 (return r[t]), paying `cost` per unit of position change."""
    pos = np.asarray(pos, dtype=float)
    changes = np.abs(np.diff(np.concatenate([[prev0], pos])))
    sret = pos * r - cost * changes
    eq = np.cumprod(1 + sret)
    total = float(eq[-1] - 1) if len(eq) else 0.0
    years = len(sret) / ppy if ppy > 0 else 0
    ann = float((1 + total) ** (1 / years) - 1) if years > 0 and total > -1 else None
    sd = float(np.std(sret))
    return {"total_return": total, "annual_return": ann,
            "sharpe": float(np.mean(sret) / sd * math.sqrt(ppy)) if sd > 0 else None,
            "max_drawdown": float(np.min(eq / np.maximum.accumulate(eq) - 1)) if len(eq) else 0.0,
            "trades": int((changes > 0).sum()), "exposure": float(np.mean(pos != 0)) if len(pos) else 0.0}, eq


def reinforcement(job):
    """An agent trades one price column of the data set, one row at a time, on a simulation: at row t it picks a position
    (flat, long, or short unless long-only) held until row t+1, and is rewarded by that change of the price minus the
    trading costs. It sees the chosen features (scaled with the training part) and its current position.
    Learns on the training part only (`episodes` passes); judged on the later test part it never trained on, against
    buy & hold, staying flat, and random agents that trade as often. Nothing is sent anywhere: no orders exist here."""
    import numpy as np
    import pandas as pd
    progress("load", 3)
    j2 = dict(job, target_transform="raw", horizon=0)
    X, price, dates, names, _ = frame(j2, pd, np, target_required=False)
    if price is None:
        raise ValueError("pick the price column the agent trades")
    ok = (X.notna().all(axis=1) & price.notna() & (price > 0)).to_numpy()
    X, price, dates = X[ok].reset_index(drop=True), price[ok].reset_index(drop=True).to_numpy(float), dates[ok].reset_index(drop=True)
    n, F = X.shape
    if n < 200:
        raise ValueError(f"only {n} usable rows: too few for a trading simulation")
    p = job.get("params", {})
    seed = int(job.get("seed", 42))
    rng = np.random.default_rng(seed)
    method = job.get("algorithm", "q_learning")
    long_only = str(p.get("positions", "long_short")) == "long_only"
    pos_of = np.array([0.0, 1.0] if long_only else [0.0, 1.0, -1.0])
    A = len(pos_of)
    cost = float(p.get("cost_bp", 2.0)) / 1e4
    episodes = int(p.get("episodes", 30))
    gamma = float(p.get("gamma", 0.9))
    test_size = min(max(float(job.get("test_size", 0.2)), 0.05), 0.5)
    cut = int(round(n * (1 - test_size)))
    r = np.append(price[1:] / price[:-1] - 1, 0.0)                 # r[t]: row t → t+1
    mu, sd = X.iloc[:cut].mean(), X.iloc[:cut].std().replace(0, 1)
    S = np.clip(((X - mu) / sd).to_numpy(dtype=np.float32), -5, 5)
    try:   # rows per year, from the dates (daily ≈ 260, weekly 52…); 252 when the file has no dates
        span_years = (pd.Timestamp(dates.iloc[-1]) - pd.Timestamp(dates.iloc[0])).days / 365.25 if hasattr(dates.iloc[0], "year") else 0
    except Exception:
        span_years = 0
    ppy = n / span_years if span_years > 0.1 else 252.0
    emit(event="log", text=f"{n} rows, {F} features, train {cut} / test {n - cut}, {A} positions, cost {cost * 1e4:.1f} bp, ~{ppy:.0f} rows a year")
    progress("prepare", 6)
    reward_scale = 100.0

    def reward(t, a, cur):
        return reward_scale * (pos_of[a] * r[t] - cost * abs(pos_of[a] - pos_of[cur]))

    # ---------------------------------------------------------------- agents: act(t, cur, greedy) and learn per episode
    if method == "q_learning":
        k = min(F, 4)
        bins = int(p.get("bins", 3))
        edges = [np.quantile(S[:cut, i], np.linspace(0, 1, bins + 1)[1:-1]) for i in range(k)]
        state = np.zeros(n, dtype=int)
        for i in range(k):
            state = state * bins + np.searchsorted(edges[i], S[:, i])
        if F > k:
            emit(event="log", text=f"tabular Q-learning uses the first {k} features only: {', '.join(names[:k])}")
        Q = np.zeros((bins ** k * A, A))
        alpha = float(p.get("alpha", 0.1))
        def act(t, cur, greedy, eps=0.0):
            if not greedy and rng.random() < eps:
                return int(rng.integers(A))
            q = Q[state[t] * A + cur]
            return int(rng.choice(np.flatnonzero(q == q.max())))
        def episode(ep):
            eps = max(0.02, 0.5 * (1 - ep / max(episodes - 1, 1)))
            cur, tot = 0, 0.0
            for t in range(cut - 1):
                a = act(t, cur, False, eps)
                rw = reward(t, a, cur)
                s0, s1 = state[t] * A + cur, state[t + 1] * A + a
                Q[s0, a] += alpha * (rw + gamma * Q[s1].max() - Q[s0, a])
                cur, tot = a, tot + rw
            return tot
        model_state = lambda: {"Q": Q, "edges": edges, "features_used": names[:k]}
    else:
        import torch
        import torch.nn as nn
        torch.set_num_threads(4)
        torch.manual_seed(seed)
        St = torch.from_numpy(S)
        eye = torch.eye(A)
        def obs(ts, curs):
            return torch.cat([St[ts], eye[curs]], dim=1)
        lr = float(p.get("learning_rate", 0.001))
        if method == "dqn":
            def net():
                return nn.Sequential(nn.Linear(F + A, 64), nn.ReLU(), nn.Linear(64, 64), nn.ReLU(), nn.Linear(64, A))
            qnet, target = net(), net()
            target.load_state_dict(qnet.state_dict())
            opt = torch.optim.Adam(qnet.parameters(), lr=lr)
            cap = 50000
            buf_t, buf_c, buf_a = (np.zeros(cap, dtype=np.int64) for _ in range(3))
            buf_r = np.zeros(cap, dtype=np.float32)
            fill, ptr, steps = 0, 0, 0
            def act(t, cur, greedy, eps=0.0):
                if not greedy and rng.random() < eps:
                    return int(rng.integers(A))
                with torch.no_grad():
                    return int(qnet(obs([t], [cur])).argmax())
            def episode(ep):
                nonlocal fill, ptr, steps
                eps = max(0.02, 1.0 - ep / max(episodes * 0.7, 1))
                cur, tot = 0, 0.0
                for t in range(cut - 1):
                    a = act(t, cur, False, eps)
                    rw = reward(t, a, cur)
                    buf_t[ptr], buf_c[ptr], buf_a[ptr], buf_r[ptr] = t, cur, a, rw
                    ptr, fill = (ptr + 1) % cap, min(fill + 1, cap)
                    cur, tot, steps = a, tot + rw, steps + 1
                    if fill >= 500 and steps % 4 == 0:
                        b = rng.integers(0, fill, 64)
                        bt, bc, ba = torch.from_numpy(buf_t[b]), torch.from_numpy(buf_c[b]), torch.from_numpy(buf_a[b])
                        br = torch.from_numpy(buf_r[b])
                        with torch.no_grad():   # double DQN: the online net picks, the target net values
                            nxt = obs(bt + 1, ba)
                            best = qnet(nxt).argmax(dim=1, keepdim=True)
                            y = br + gamma * target(nxt).gather(1, best).squeeze(1)
                        q = qnet(obs(bt, bc)).gather(1, ba.unsqueeze(1)).squeeze(1)
                        loss = nn.functional.smooth_l1_loss(q, y)
                        opt.zero_grad(); loss.backward(); opt.step()
                    if steps % 1000 == 0:
                        target.load_state_dict(qnet.state_dict())
                return tot
            model_state = lambda: {"qnet": qnet.state_dict()}
        elif method == "a2c":
            class AC(nn.Module):
                def __init__(self):
                    super().__init__()
                    self.body = nn.Sequential(nn.Linear(F + A, 64), nn.Tanh(), nn.Linear(64, 64), nn.Tanh())
                    self.pi = nn.Linear(64, A)
                    self.v = nn.Linear(64, 1)
                def forward(self, x):
                    h = self.body(x)
                    return self.pi(h), self.v(h).squeeze(-1)
            ac = AC()
            opt = torch.optim.Adam(ac.parameters(), lr=lr)
            ent_coef = float(p.get("entropy", 0.01))
            nstep = 32
            def act(t, cur, greedy, eps=0.0):
                with torch.no_grad():
                    logits, _ = ac(obs([t], [cur]))
                    if greedy:
                        return int(logits.argmax())
                    return int(torch.distributions.Categorical(logits=logits).sample())
            def episode(ep):
                cur, tot, t = 0, 0.0, 0
                while t < cut - 1:
                    ts, cs, acts, rws = [], [], [], []
                    for _ in range(nstep):
                        if t >= cut - 1:
                            break
                        a = act(t, cur, False)
                        rw = reward(t, a, cur)
                        ts.append(t); cs.append(cur); acts.append(a); rws.append(rw)
                        cur, tot, t = a, tot + rw, t + 1
                    o = obs(ts, cs)
                    logits, v = ac(o)
                    with torch.no_grad():
                        _, v_last = ac(obs([min(t, n - 1)], [cur]))
                        ret = float(v_last[0]) if t < cut - 1 else 0.0
                        rets = []
                        for rw in reversed(rws):
                            ret = rw + gamma * ret
                            rets.append(ret)
                        rets = torch.tensor(rets[::-1], dtype=torch.float32)
                    adv = rets - v.detach()
                    dist = torch.distributions.Categorical(logits=logits)
                    at = torch.tensor(acts)
                    loss = -(dist.log_prob(at) * adv).mean() + 0.5 * ((rets - v) ** 2).mean() - ent_coef * dist.entropy().mean()
                    opt.zero_grad(); loss.backward()
                    nn.utils.clip_grad_norm_(ac.parameters(), 0.5)
                    opt.step()
                return tot
            model_state = lambda: {"actor_critic": ac.state_dict()}
        else:
            raise ValueError(f"unknown method '{method}'")

    def greedy_run(t0, t1, cur=0):
        pos, acts = [], []
        for t in range(t0, t1):
            a = act(t, cur, True)
            acts.append(a); pos.append(pos_of[a]); cur = a
        return np.array(pos), acts, cur

    history = []
    for ep in range(episodes):
        tot = episode(ep)
        ptr_, _, _ = greedy_run(0, cut - 1)
        pte, _, _ = greedy_run(cut, n - 1)
        history.append({"episode": ep + 1, "reward": tot / reward_scale,
                        "train_return": perf(ptr_, r[:cut - 1], 0, cost, ppy, np)[0]["total_return"],
                        "test_return": perf(pte, r[cut:n - 1], 0, cost, ppy, np)[0]["total_return"]})
        point("return", ep + 1, train=history[-1]["train_return"], test=history[-1]["test_return"])
        emit(event="progress", stage=f"episode {ep + 1}/{episodes} · train {history[-1]['train_return'] * 100:+.1f}%", pct=round(8 + 82 * (ep + 1) / episodes, 1))

    progress("evaluate", 92)
    pos_tr, _, _ = greedy_run(0, cut - 1)
    pos_te, acts_te, last_cur = greedy_run(cut, n - 1)
    r_te = r[cut:n - 1]
    agent, eq_agent = perf(pos_te, r_te, 0, cost, ppy, np)
    train_m, _ = perf(pos_tr, r[:cut - 1], 0, cost, ppy, np)
    bh, eq_bh = perf(np.ones(len(r_te)), r_te, 0, cost, ppy, np)
    flat, _ = perf(np.zeros(len(r_te)), r_te, 0, cost, ppy, np)
    # random agents trading as often as the agent did
    q = max(agent["trades"] / max(len(r_te), 1), 1 / max(len(r_te), 1))
    rand = []
    for _ in range(50):
        cur, pr = 0, []
        for t in range(len(r_te)):
            if rng.random() < q:
                cur = int(rng.integers(A))
            pr.append(pos_of[cur])
        rand.append(perf(np.array(pr), r_te, 0, cost, ppy, np)[0]["total_return"])
    rand = np.array(rand)
    a_now = act(n - 1, last_cur, True)
    names_pos = ["flat", "long", "short"]
    out = pd.DataFrame({"date": dates.iloc[cut + 1:n].astype(str).values, "price": price[cut + 1:n], "position": pos_te,
                        "agent": eq_agent, "buy_hold": eq_bh})
    out.to_csv(os.path.join(RUN, "equity.csv"), index=False)
    import joblib
    joblib.dump({"method": method, "state": model_state(), "features": names, "mean": mu.tolist(), "std": sd.tolist(),
                 "positions": pos_of.tolist(), "job": job}, os.path.join(RUN, "model.joblib"))
    res = {"family": "reinforcement", "method": method, "rows": n, "features": names, "price": job.get("target"),
           "positions": "long_only" if long_only else "long_short", "cost_bp": cost * 1e4, "rows_per_year": ppy,
           "train_period": [str(dates.iloc[0]), str(dates.iloc[cut - 1])], "test_period": [str(dates.iloc[cut]), str(dates.iloc[n - 1])],
           "history": history, "test": agent, "train": train_m,
           "benchmarks": {"buy_hold": bh, "flat": flat,
                          "random": {"mean": float(rand.mean()), "p95": float(np.quantile(rand, 0.95)), "p05": float(np.quantile(rand, 0.05)),
                                     "beaten": float((rand < agent["total_return"]).mean()), "runs": len(rand)}},
           "latest": {"date": str(dates.iloc[n - 1]), "action": names_pos[int(np.flatnonzero(np.array([0, 1, -1]) == pos_of[a_now])[0])]},
           "seconds": time.time() - T0, "versions": versions()}
    if method != "q_learning":
        import torch
        res["versions"]["torch"] = torch.__version__
    with open(os.path.join(RUN, "result.json"), "w", encoding="utf-8") as f:
        json.dump(clean(res), f, indent=1)
    progress("done", 100)


def versions():
    v = {"python": sys.version.split()[0]}
    for mod in ("numpy", "pandas", "sklearn", "lightgbm", "xgboost"):
        if mod in sys.modules:
            v[mod] = getattr(sys.modules[mod], "__version__", "?")
    return v


def refs_only(job):
    """--refs: a finished supervised or semi-supervised run made before the references were written gets them now —
    no_skill and persistence next to each test row, the no-skill value, persistence's scores, the data fingerprint —
    only if the data set still gives exactly the run's test rows: the same dates, the same actual values, and a
    rebuilt no-skill that scores exactly what the run recorded. Otherwise nothing is touched and refs.json says why.
    No model is trained; the run's own predictions stay as they are."""
    import numpy as np
    import pandas as pd
    fam = job.get("family", "supervised")
    res_path, pred_path = os.path.join(RUN, "result.json"), os.path.join(RUN, "predictions.csv")
    with open(res_path, encoding="utf-8") as f:
        res = json.load(f)
    out = pd.read_csv(pred_path)

    def verdict(ok, why=""):
        with open(os.path.join(RUN, "refs.json"), "w", encoding="utf-8") as f:
            json.dump({"ok": ok, "why": why, "at": time.strftime("%Y-%m-%dT%H:%M:%S")}, f)
        emit(event="log", text=("references added" if ok else "references not added: " + why))

    task = res.get("task", "classification")
    transform = job.get("target_transform", "raw")
    if fam == "supervised":
        X, y, dates, _, _, _, transform = prepare(job, pd, np)
        n = len(X)
        test_size = min(max(float(job.get("test_size", 0.2)), 0.05), 0.5)
        cut = int(round(n * (1 - test_size)))
        h = max(0, int(job.get("horizon", 0)))
        tr_end = cut - h
        y_tr, y_te = y.iloc[:tr_end], y.iloc[cut:]
        cap = SUPERVISED_ROW_CAP.get(job.get("algorithm", ""))
        if cap and len(y_tr) > cap:
            y_tr = y_tr.iloc[-cap:]
        if task == "classification":
            y_tr, y_te = y_tr.astype(int), y_te.astype(int)
        dates_te = dates.iloc[cut:].astype(str).values
        pers_te = PERSIST.iloc[cut:].to_numpy(dtype=float) if PERSIST is not None else np.full(len(y_te), np.nan)
        if task == "classification":
            base = {"rule": "majority", "class": int(y_tr.mode().iloc[0])}
            const = base["class"]
        else:
            mean = float(y_tr.mean())
            base = {"rule": "mean_direction" if transform in ("change", "pct_change") else "mean", "value": mean}
            const = mean
    elif fam == "semi_supervised":
        S = semi_split(job, pd, np)
        te, back, yr, dr, keep, y_semi = S["te"], S["back"], S["yr"], S["dr"], S["keep"], S["y_semi"]
        y_te = pd.Series(back(yr.iloc[te].astype(int)))
        dates_te = dr.iloc[te].astype(str).values
        pr_all = S["persist"].iloc[S["rows"]].reset_index(drop=True) if S["persist"] is not None else None
        pers_te = pr_all.iloc[te].to_numpy(dtype=float) if pr_all is not None else np.full(len(te), np.nan)
        const = int(back([pd.Series(y_semi[keep]).mode().iloc[0]])[0])
        base = {"rule": "majority", "class": const}
        task, transform = "classification", S["transform"]
    else:
        return verdict(False, f"references are added to supervised and semi-supervised runs only, not {fam}")

    # the run's own test rows, rebuilt: they must match what it wrote, or the data set has changed since
    if len(dates_te) != len(out):
        return verdict(False, f"the data set now gives {len(dates_te)} test rows, the run had {len(out)}: it was rebuilt since")
    if not (pd.Series(dates_te).values == out["date"].astype(str).values).all():
        return verdict(False, "the test rows' dates differ from the run's: the data set was rebuilt since")
    if not np.allclose(np.asarray(y_te, dtype=float), out["actual"].to_numpy(dtype=float), rtol=0, atol=1e-9):
        return verdict(False, "the test rows' actual values differ from the run's: the data set was rebuilt since")
    old = res.get("baseline", {})
    if task == "classification" and old.get("class") is not None and int(old["class"]) != int(const):
        return verdict(False, "the rebuilt no-skill class differs from the run's")
    check = scores(task, np.asarray(y_te), np.full(len(out), const), transform=transform)
    key = "accuracy" if task == "classification" else "mae"
    if key in old and abs(float(old[key]) - float(check[key])) > 1e-9 * max(1.0, abs(float(old[key]))):
        return verdict(False, f"the rebuilt no-skill {key} ({check[key]:.6g}) differs from the run's ({float(old[key]):.6g})")

    # all checked: the references, next to the run's untouched predictions; the files before kept beside
    import shutil
    shutil.copyfile(pred_path, os.path.join(RUN, "predictions.before-refs.csv"))
    shutil.copyfile(res_path, os.path.join(RUN, "result.before-refs.json"))
    persistence = reference_columns(out, task, transform, {**old, **base}, pers_te, np)
    out.to_csv(pred_path, index=False)
    res["baseline"] = {**old, **{k: v for k, v in base.items() if k not in old}}
    res["persistence"] = persistence
    res["references_added"] = time.strftime("%Y-%m-%d")
    res["data"] = data_fingerprint()
    with open(res_path, "w", encoding="utf-8") as f:
        json.dump(clean(res), f, indent=1)
    verdict(True)


def data_fingerprint():
    """The data set as the run read it: SHA-256 of the file, its size, rows and first and last date (frame())."""
    import hashlib
    h = hashlib.sha256()
    with open(DATA, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return {"sha256": h.hexdigest(), "bytes": os.path.getsize(DATA), **DATA_INFO}


def add_data_info():
    """Written into result.json once the family is done: ml_compare ranks together only runs on the same data version,
    and a re-run knows whether it can give the same result."""
    path = os.path.join(RUN, "result.json")
    with open(path, encoding="utf-8") as f:
        res = json.load(f)
    res["data"] = data_fingerprint()
    with open(path, "w", encoding="utf-8") as f:
        json.dump(clean(res), f, indent=1)


def main():
    try:
        job = load_job()
        fam = job.get("family", "supervised")
        if "--refs" in FLAGS:
            refs_only(job)
            emit(event="done")
            return
        if fam == "supervised":
            supervised(job)
        elif fam == "unsupervised":
            unsupervised(job)
        elif fam == "semi_supervised":
            semi_supervised(job)
        elif fam == "self_supervised":
            self_supervised(job)
        elif fam == "reinforcement":
            reinforcement(job)
        else:
            raise ValueError(f"the family '{fam}' is not available yet")
        add_data_info()
        emit(event="done")
    except Exception as e:  # the app shows the message; the traceback goes to the run's log
        with open(os.path.join(RUN, "error.txt"), "w", encoding="utf-8") as f:
            f.write(traceback.format_exc())
        msg = str(e) or e.__class__.__name__
        if isinstance(e, (ImportError, OSError)) and "libomp" in msg:
            msg = "this library needs OpenMP (libomp), which is not on this computer: install it with Homebrew (brew install libomp) or pick another algorithm"
        emit(event="error", message=msg)
        sys.exit(1)


if __name__ == "__main__":
    main()
