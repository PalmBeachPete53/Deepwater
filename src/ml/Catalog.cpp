#include "ml/Catalog.h"
#include <QCoreApplication>

namespace ml {
QList<Algo> algos() {   // built at each call: the labels follow the interface language
  const QList<Algo> a{
      {"linear", QCoreApplication::translate("ai", "Linear / logistic"), QCoreApplication::translate("ai", "Weighted sum of the features (logistic regression for classes, ridge for values). Fast, readable, a good first baseline."),
       {{"regularization", QCoreApplication::translate("ai", "Regularization"), 'd', 0.001, 100, 1, 0.1, {}}}, {}},
      {"random_forest", QCoreApplication::translate("ai", "Random forest"), QCoreApplication::translate("ai", "Hundreds of decision trees on random samples, averaged. Robust, little tuning, catches non-linear effects."),
       {{"n_estimators", QCoreApplication::translate("ai", "Trees"), 'i', 10, 2000, 300, 50, {}}, {"max_depth", QCoreApplication::translate("ai", "Max depth (0 = none)"), 'i', 0, 50, 0, 1, {}},
        {"min_samples_leaf", QCoreApplication::translate("ai", "Min rows per leaf"), 'i', 1, 500, 5, 1, {}}}, {}},
      {"hist_gb", QCoreApplication::translate("ai", "Gradient boosting"), QCoreApplication::translate("ai", "Trees added one by one, each correcting the previous ones (scikit-learn's histogram boosting). Often the most accurate on tables."),
       {{"n_estimators", QCoreApplication::translate("ai", "Iterations"), 'i', 10, 5000, 300, 50, {}}, {"learning_rate", QCoreApplication::translate("ai", "Learning rate"), 'd', 0.001, 1, 0.05, 0.01, {}},
        {"max_depth", QCoreApplication::translate("ai", "Max depth (0 = none)"), 'i', 0, 30, 0, 1, {}}}, {}},
      {"lightgbm", "LightGBM", QCoreApplication::translate("ai", "Microsoft's gradient boosting: fast on large tables."),
       {{"n_estimators", QCoreApplication::translate("ai", "Iterations"), 'i', 10, 5000, 300, 50, {}}, {"learning_rate", QCoreApplication::translate("ai", "Learning rate"), 'd', 0.001, 1, 0.05, 0.01, {}},
        {"max_depth", QCoreApplication::translate("ai", "Max depth (0 = none)"), 'i', 0, 30, 0, 1, {}}}, "lightgbm"},
      {"xgboost", "XGBoost", QCoreApplication::translate("ai", "The classic gradient boosting library."),
       {{"n_estimators", QCoreApplication::translate("ai", "Iterations"), 'i', 10, 5000, 300, 50, {}}, {"learning_rate", QCoreApplication::translate("ai", "Learning rate"), 'd', 0.001, 1, 0.05, 0.01, {}},
        {"max_depth", QCoreApplication::translate("ai", "Max depth (0 = 6)"), 'i', 0, 30, 6, 1, {}}}, "xgboost"},
      {"knn", QCoreApplication::translate("ai", "Nearest neighbours"), QCoreApplication::translate("ai", "Answers like the most similar past rows. Simple; needs features on comparable scales (done for you)."),
       {{"n_neighbors", QCoreApplication::translate("ai", "Neighbours"), 'i', 1, 500, 15, 1, {}}}, {}},
      {"svm", QCoreApplication::translate("ai", "Support vector machine"), QCoreApplication::translate("ai", "Finds the widest boundary between classes (or a tube around values). Good on small tables, slow on big ones."),
       {{"c", QCoreApplication::translate("ai", "C (strictness)"), 'd', 0.01, 1000, 1, 0.5, {}}}, {}},
      {"mlp", QCoreApplication::translate("ai", "Neural network (MLP)"), QCoreApplication::translate("ai", "Layers of neurons trained by back-propagation, with early stopping on a held-out part of the training rows."),
       {{"hidden_layers", QCoreApplication::translate("ai", "Hidden layers"), 's', 1, 1024, 0, 0, "64,32"}, {"max_iter", QCoreApplication::translate("ai", "Max epochs"), 'i', 20, 5000, 500, 50, {}}}, {}},
      // unsupervised (family 1): groups, unusual rows, fewer dimensions
      {"kmeans", "k-means", QCoreApplication::translate("ai", "Splits the rows into k groups around k centres. Fast; groups of similar size and shape. The scan compares k values."),
       {{"n_clusters", QCoreApplication::translate("ai", "Groups (k)"), 'i', 2, 20, 4, 1, {}}, {"scan_max", QCoreApplication::translate("ai", "Compare k up to (0 = no)"), 'i', 0, 15, 8, 1, {}}}, {}, 1},
      {"gmm", QCoreApplication::translate("ai", "Gaussian mixture"), QCoreApplication::translate("ai", "Groups as overlapping bell curves: each row gets the probability of belonging to each group. Suits market regimes."),
       {{"n_clusters", QCoreApplication::translate("ai", "Groups"), 'i', 2, 20, 3, 1, {}}, {"covariance", QCoreApplication::translate("ai", "Shape"), 'c', 0, 0, 0, 0, "full", {"full", "diag", "tied", "spherical"}}}, {}, 1},
      {"agglomerative", QCoreApplication::translate("ai", "Hierarchical"), QCoreApplication::translate("ai", "Merges the closest rows step by step until k groups remain. Latest 8,000 rows at most."),
       {{"n_clusters", QCoreApplication::translate("ai", "Groups"), 'i', 2, 20, 4, 1, {}}, {"linkage", QCoreApplication::translate("ai", "Linkage"), 'c', 0, 0, 0, 0, "ward", {"ward", "complete", "average", "single"}}}, {}, 1},
      {"dbscan", "DBSCAN", QCoreApplication::translate("ai", "Groups dense areas and leaves isolated rows aside as noise; finds the number of groups itself."),
       {{"eps", QCoreApplication::translate("ai", "Radius (in standard deviations)"), 'd', 0.05, 20, 1.5, 0.1, {}}, {"min_samples", QCoreApplication::translate("ai", "Min rows per group"), 'i', 2, 500, 10, 1, {}}}, {}, 1},
      {"isolation_forest", "Isolation forest", QCoreApplication::translate("ai", "Flags the rows that random splits isolate fastest: the unusual days."),
       {{"contamination", QCoreApplication::translate("ai", "Share flagged as unusual"), 'd', 0.001, 0.5, 0.02, 0.005, {}}, {"n_estimators", QCoreApplication::translate("ai", "Trees"), 'i', 50, 2000, 300, 50, {}}}, {}, 1},
      {"lof", QCoreApplication::translate("ai", "Local outlier factor"), QCoreApplication::translate("ai", "Flags the rows that sit in an emptier area than their neighbours."),
       {{"n_neighbors", QCoreApplication::translate("ai", "Neighbours"), 'i', 5, 500, 20, 1, {}}, {"contamination", QCoreApplication::translate("ai", "Share flagged as unusual"), 'd', 0.001, 0.5, 0.02, 0.005, {}}}, {}, 1},
      {"pca", QCoreApplication::translate("ai", "Principal components (PCA)"), QCoreApplication::translate("ai", "Sums up many features in a few independent axes, and says what each axis is made of."),
       {{"n_components", QCoreApplication::translate("ai", "Components"), 'i', 1, 20, 3, 1, {}}}, {}, 1},
      {"tsne", "t-SNE", QCoreApplication::translate("ai", "Draws a map where similar rows sit close together. For looking, not for measuring. Latest 5,000 rows at most."),
       {{"perplexity", QCoreApplication::translate("ai", "Perplexity"), 'd', 5, 100, 30, 5, {}}}, {}, 1},
      // semi-supervised (family 2): few labels kept, the rest of the training rows unlabelled
      {"self_training", QCoreApplication::translate("ai", "Self-training"), QCoreApplication::translate("ai", "A model learns from the labels it has, labels the rows it is surest of, and learns again with them."),
       {{"labelled_share", QCoreApplication::translate("ai", "Training rows that keep their label"), 'd', 0.005, 1, 0.1, 0.05, {}},
        {"base", QCoreApplication::translate("ai", "Model"), 'c', 0, 0, 0, 0, "gradient_boosting", {"gradient_boosting", "random_forest", "logistic"}},
        {"threshold", QCoreApplication::translate("ai", "Confidence to self-label"), 'd', 0.5, 0.99, 0.8, 0.05, {}}, {"max_iter", QCoreApplication::translate("ai", "Rounds"), 'i', 1, 50, 10, 1, {}}}, {}, 2},
      {"label_spreading", QCoreApplication::translate("ai", "Label spreading"), QCoreApplication::translate("ai", "Labels spread to the nearest unlabelled rows through a graph of neighbours, softly (a label may be revised)."),
       {{"labelled_share", QCoreApplication::translate("ai", "Training rows that keep their label"), 'd', 0.005, 1, 0.1, 0.05, {}},
        {"n_neighbors", QCoreApplication::translate("ai", "Neighbours"), 'i', 2, 100, 10, 1, {}}, {"alpha", QCoreApplication::translate("ai", "Softness (0 = keep given labels)"), 'd', 0, 0.99, 0.2, 0.05, {}}}, {}, 2},
      {"label_propagation", QCoreApplication::translate("ai", "Label propagation"), QCoreApplication::translate("ai", "Like label spreading, but the given labels never change."),
       {{"labelled_share", QCoreApplication::translate("ai", "Training rows that keep their label"), 'd', 0.005, 1, 0.1, 0.05, {}},
        {"n_neighbors", QCoreApplication::translate("ai", "Neighbours"), 'i', 2, 100, 10, 1, {}}}, {}, 2},
      // self-supervised (family 3): PyTorch, windows of rows, no target
      {"masked_autoencoder", QCoreApplication::translate("ai", "Masked autoencoder"), QCoreApplication::translate("ai", "Hides values at random in a window of rows and learns to rebuild them. Its rebuild error also flags unusual windows."),
       {{"window", QCoreApplication::translate("ai", "Window (rows)"), 'i', 5, 120, 20, 1, {}}, {"embedding", QCoreApplication::translate("ai", "Embedding size"), 'i', 2, 128, 16, 1, {}},
        {"epochs", QCoreApplication::translate("ai", "Epochs"), 'i', 1, 500, 40, 5, {}}, {"learning_rate", QCoreApplication::translate("ai", "Learning rate"), 'd', 0.001, 0.1, 0.001, 0.001, {}},
        {"mask_ratio", QCoreApplication::translate("ai", "Share hidden"), 'd', 0.05, 0.9, 0.3, 0.05, {}}}, "torch", 3},
      {"contrastive", QCoreApplication::translate("ai", "Contrastive (SimCLR)"), QCoreApplication::translate("ai", "Two noisy copies of the same window must land close together, and apart from other windows."),
       {{"window", QCoreApplication::translate("ai", "Window (rows)"), 'i', 5, 120, 20, 1, {}}, {"embedding", QCoreApplication::translate("ai", "Embedding size"), 'i', 2, 128, 16, 1, {}},
        {"epochs", QCoreApplication::translate("ai", "Epochs"), 'i', 1, 500, 40, 5, {}}, {"learning_rate", QCoreApplication::translate("ai", "Learning rate"), 'd', 0.001, 0.1, 0.001, 0.001, {}},
        {"noise", QCoreApplication::translate("ai", "Noise"), 'd', 0, 1, 0.1, 0.05, {}}, {"temperature", QCoreApplication::translate("ai", "Temperature"), 'd', 0.05, 1, 0.2, 0.05, {}}}, "torch", 3},
      {"next_step", QCoreApplication::translate("ai", "Next step"), QCoreApplication::translate("ai", "Learns to predict the next row from the window before it. Its error flags windows that did not go as usual."),
       {{"window", QCoreApplication::translate("ai", "Window (rows)"), 'i', 5, 120, 20, 1, {}}, {"embedding", QCoreApplication::translate("ai", "Embedding size"), 'i', 2, 128, 16, 1, {}},
        {"epochs", QCoreApplication::translate("ai", "Epochs"), 'i', 1, 500, 40, 5, {}}, {"learning_rate", QCoreApplication::translate("ai", "Learning rate"), 'd', 0.001, 0.1, 0.001, 0.001, {}}}, "torch", 3},
      // reinforcement (family 4): an agent trades the price column on a simulation (no order anywhere)
      {"q_learning", QCoreApplication::translate("ai", "Q-learning (table)"), QCoreApplication::translate("ai", "Keeps a table of what each action is worth in each situation (the first 4 features cut into bins). Simple and readable; no PyTorch."),
       {{"episodes", QCoreApplication::translate("ai", "Episodes (passes over the training rows)"), 'i', 1, 500, 50, 5, {}}, {"bins", QCoreApplication::translate("ai", "Bins per feature"), 'i', 2, 5, 3, 1, {}},
        {"alpha", QCoreApplication::translate("ai", "Learning rate"), 'd', 0.01, 1, 0.1, 0.05, {}}, {"gamma", QCoreApplication::translate("ai", "Patience (discount)"), 'd', 0, 0.99, 0.9, 0.05, {}},
        {"cost_bp", QCoreApplication::translate("ai", "Costs (bp per position change)"), 'd', 0, 100, 2, 0.5, {}},
        {"positions", QCoreApplication::translate("ai", "Positions"), 'c', 0, 0, 0, 0, "long_short", {"long_short", "long_only"}}}, {}, 4},
      {"dqn", QCoreApplication::translate("ai", "Deep Q-network (DQN)"), QCoreApplication::translate("ai", "A neural network estimates what each action is worth, learning from a memory of past steps (double DQN)."),
       {{"episodes", QCoreApplication::translate("ai", "Episodes (passes over the training rows)"), 'i', 1, 200, 15, 1, {}}, {"learning_rate", QCoreApplication::translate("ai", "Learning rate"), 'd', 0.001, 0.1, 0.001, 0.001, {}},
        {"gamma", QCoreApplication::translate("ai", "Patience (discount)"), 'd', 0, 0.99, 0.9, 0.05, {}}, {"cost_bp", QCoreApplication::translate("ai", "Costs (bp per position change)"), 'd', 0, 100, 2, 0.5, {}},
        {"positions", QCoreApplication::translate("ai", "Positions"), 'c', 0, 0, 0, 0, "long_short", {"long_short", "long_only"}}}, "torch", 4},
      {"a2c", QCoreApplication::translate("ai", "Actor-critic (A2C)"), QCoreApplication::translate("ai", "One network chooses the action (actor), another judges the situation (critic); learns its strategy directly."),
       {{"episodes", QCoreApplication::translate("ai", "Episodes (passes over the training rows)"), 'i', 1, 500, 20, 5, {}}, {"learning_rate", QCoreApplication::translate("ai", "Learning rate"), 'd', 0.001, 0.1, 0.001, 0.001, {}},
        {"entropy", QCoreApplication::translate("ai", "Exploration (entropy)"), 'd', 0, 0.2, 0.01, 0.005, {}}, {"gamma", QCoreApplication::translate("ai", "Patience (discount)"), 'd', 0, 0.99, 0.9, 0.05, {}},
        {"cost_bp", QCoreApplication::translate("ai", "Costs (bp per position change)"), 'd', 0, 100, 2, 0.5, {}},
        {"positions", QCoreApplication::translate("ai", "Positions"), 'c', 0, 0, 0, 0, "long_short", {"long_short", "long_only"}}}, "torch", 4},
  };
  return a;
}
QList<Algo> algosFor(int family) { QList<Algo> o; for (const auto& a : algos()) if (a.family == family) o << a; return o; }
Algo algoById(const QString& id) { for (const auto& a : algos()) if (a.id == id) return a; return Algo{}; }
QString familyId(int family) {
  static const char* ids[] = {"supervised", "unsupervised", "semi_supervised", "self_supervised", "reinforcement"};
  return family >= 0 && family < 5 ? ids[family] : QString();
}
int familyOf(const QString& id) {
  for (int f = 0; f < 5; ++f) if (familyId(f) == id) return f;
  return -1;
}
}  // namespace ml
