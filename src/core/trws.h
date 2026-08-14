#ifndef TRWS_H
#define TRWS_H

#include <vector>
#include <algorithm>

struct TRWSResult {
    std::vector<int> labels;               // 0..K-1
    double energy = 0.0;
    double reward = 0.0;                   // = -energy
    std::vector<double> lowerBoundHistory;
    int iterations = 0;
};

class TRWS {
public:
    struct Options {
        int maxIter = 200;
        double tol = 1e-9;
        int stallIters = 10;
    };

    TRWS(
        int numNodes,
        int numLabels,
        const std::vector<std::vector<double>>& unaryCosts,
        const std::vector<std::pair<int,int>>& edges,
        const std::vector<std::vector<double>>& pairCosts,
        const std::vector<int>& order,
        const Options& opts);

    TRWSResult run();

private:
    struct IncEdge {
        int edgeId;
        int neighbor;
        int endpointFlag; // 1 if node == edges[e].first, 2 if node == edges[e].second
    };

    int N, K, M;
    std::vector<std::vector<double>> unaryCosts;
    std::vector<std::pair<int,int>> edges;
    std::vector<std::vector<double>> pairCosts;
    std::vector<int> order;
    Options opts;

    std::vector<std::vector<IncEdge>> inc;
    std::vector<double> gammaNode;   // gamma_{s->t} = 1 / n_s

    std::vector<double> msg12;       // M x K
    std::vector<double> msg21;       // M x K

    static void addVectorInPlace(std::vector<double>& dst, const double* src, int n) {
        for (int i = 0; i < n; ++i) dst[i] += src[i];
    }

    static double minValue(const std::vector<double>& v) {
        return *std::min_element(v.begin(), v.end());
    }

    void buildIncidentList();
    void buildGamma();
    std::vector<int> decodeLabels() const;
    double computeEnergy(const std::vector<int>& labels) const;
};

#endif // TRWS_H
