#include "trws.h"
#include <stdexcept>
#include <limits>

TRWS::TRWS(
    int numNodes,
    int numLabels,
    const std::vector<std::vector<double>>& unaryCosts,
    const std::vector<std::pair<int,int>>& edges,
    const std::vector<std::vector<double>>& pairCosts,
    const std::vector<int>& order,
    const Options& opts)
    : N(numNodes),
    K(numLabels),
    M(static_cast<int>(edges.size())),
    unaryCosts(unaryCosts),
    edges(edges),
    pairCosts(pairCosts),
    order(order),
    opts(opts)
{
    if (N <= 0) throw std::invalid_argument("N must be positive.");
    if (K <= 0) throw std::invalid_argument("K must be positive.");

    if (static_cast<int>(this->unaryCosts.size()) != N) {
        throw std::invalid_argument("unaryCosts.size() must equal N.");
    }
    for (int i = 0; i < N; ++i) {
        if (static_cast<int>(this->unaryCosts[i].size()) != K) {
            throw std::invalid_argument("Each unaryCosts[i] must have size K.");
        }
    }

    if (static_cast<int>(pairCosts.size()) != M) {
        throw std::invalid_argument("pairCosts.size() must equal edges.size().");
    }
    for (int e = 0; e < M; ++e) {
        if (static_cast<int>(pairCosts[e].size()) != K * K) {
            throw std::invalid_argument("Each pairCosts[e] must have size K*K.");
        }
        int a = edges[e].first;
        int b = edges[e].second;
        if (a < 0 || a >= N || b < 0 || b >= N) {
            throw std::invalid_argument("Edge node index out of range.");
        }
        if (a == b) {
            throw std::invalid_argument("Self-loops are not supported.");
        }
    }

    if (order.empty()) {
        this->order.resize(N);
        for (int i = 0; i < N; ++i) this->order[i] = i;
    } else {
        if (static_cast<int>(order.size()) != N) {
            throw std::invalid_argument("order.size() must be N.");
        }
        this->order = order;
        std::vector<int> seen(N, 0);
        for (int v : this->order) {
            if (v < 0 || v >= N || seen[v]) {
                throw std::invalid_argument("order must be a permutation of 0..N-1.");
            }
            seen[v] = 1;
        }
    }

    buildIncidentList();
    buildGamma();
    msg12.assign(M * K, 0.0);
    msg21.assign(M * K, 0.0);
}

TRWSResult TRWS::run() {
    TRWSResult out;
    std::vector<int> currentOrder = order;
    std::vector<int> currentPos(N);
    for (int i = 0; i < N; ++i) currentPos[currentOrder[i]] = i;

    std::vector<double> thetaHat(K, 0.0);
    std::vector<double> newMsg(K, 0.0);

    for (int it = 0; it < opts.maxIter; ++it) {
        if (opts.shouldCancel && opts.shouldCancel()) {
            out.cancelled = true;
            out.iterations = it;
            return out;
        }
        double Ebound = 0.0;

        for (int idx = 0; idx < N; ++idx) {
            if (opts.shouldCancel && opts.shouldCancel()) {
                out.cancelled = true;
                out.iterations = it;
                return out;
            }
            int s = currentOrder[idx];

            // thetaHat = unary + incoming messages
            for (int j = 0; j < K; ++j) {
                thetaHat[j] = unaryCosts[s][j];
            }

            for (const auto& item : inc[s]) {
                int e = item.edgeId;
                int endpointFlag = item.endpointFlag;

                if (endpointFlag == 1) {
                    addVectorInPlace(thetaHat, &msg21[e * K], K);
                } else {
                    addVectorInPlace(thetaHat, &msg12[e * K], K);
                }
            }

            {
                double delta = minValue(thetaHat);
                for (int j = 0; j < K; ++j) thetaHat[j] -= delta;
                Ebound += delta;
            }

            for (const auto& item : inc[s]) {
                int e = item.edgeId;
                int t = item.neighbor;
                int endpointFlag = item.endpointFlag;

                if (currentPos[s] < currentPos[t]) {
                    if (endpointFlag == 1) {
                        const double* C = pairCosts[e].data();
                        const double* Mts = &msg21[e * K];

                        for (int k = 0; k < K; ++k) {
                            double best = std::numeric_limits<double>::infinity();
                            for (int j = 0; j < K; ++j) {
                                double val = gammaNode[s] * thetaHat[j]
                                             - Mts[j]
                                             + C[j * K + k];
                                if (val < best) best = val;
                            }
                            newMsg[k] = best;
                        }

                        double delta = minValue(newMsg);
                        for (int k = 0; k < K; ++k) msg12[e * K + k] = newMsg[k] - delta;
                        Ebound += delta;

                    } else {
                        const double* C = pairCosts[e].data();
                        const double* Mts = &msg12[e * K];

                        for (int k = 0; k < K; ++k) {
                            double best = std::numeric_limits<double>::infinity();
                            for (int j = 0; j < K; ++j) {
                                double val = gammaNode[s] * thetaHat[j]
                                             - Mts[j]
                                             + C[k * K + j];
                                if (val < best) best = val;
                            }
                            newMsg[k] = best;
                        }

                        double delta = minValue(newMsg);
                        for (int k = 0; k < K; ++k) msg21[e * K + k] = newMsg[k] - delta;
                        Ebound += delta;
                    }
                }
            }
        }

        out.lowerBoundHistory.push_back(Ebound);

        if (it >= opts.stallIters) {
            double gain = out.lowerBoundHistory[it]
                          - out.lowerBoundHistory[it - opts.stallIters];
            if (gain < opts.tol) {
                out.iterations = it + 1;
                break;
            }
        }

        std::reverse(currentOrder.begin(), currentOrder.end());
        for (int i = 0; i < N; ++i) currentPos[currentOrder[i]] = i;

        if (it == opts.maxIter - 1) {
            out.iterations = opts.maxIter;
        }
    }

    if (out.iterations == 0) {
        out.iterations = static_cast<int>(out.lowerBoundHistory.size());
    }

    out.labels = decodeLabels();
    out.energy = computeEnergy(out.labels);
    out.reward = -out.energy;
    return out;
}

void TRWS::buildIncidentList() {
    inc.assign(N, {});
    for (int e = 0; e < M; ++e) {
        int a = edges[e].first;
        int b = edges[e].second;
        inc[a].push_back({e, b, 1});
        inc[b].push_back({e, a, 2});
    }
}

void TRWS::buildGamma() {
    std::vector<int> pos0(N);
    for (int i = 0; i < N; ++i) pos0[order[i]] = i;

    std::vector<int> earlierCount(N, 0);
    std::vector<int> laterCount(N, 0);

    for (int e = 0; e < M; ++e) {
        int a = edges[e].first;
        int b = edges[e].second;
        if (pos0[a] < pos0[b]) {
            laterCount[a] += 1;
            earlierCount[b] += 1;
        } else {
            laterCount[b] += 1;
            earlierCount[a] += 1;
        }
    }

    gammaNode.resize(N);
    for (int s = 0; s < N; ++s) {
        int nChains = std::max(earlierCount[s], laterCount[s]);
        if (nChains == 0) nChains = 1;
        gammaNode[s] = 1.0 / static_cast<double>(nChains);
    }
}

std::vector<int> TRWS::decodeLabels() const {
    std::vector<int> labels(N, 0);
    std::vector<int> pos0(N);
    for (int i = 0; i < N; ++i) pos0[order[i]] = i;

    std::vector<double> b(K, 0.0);

    for (int idx = 0; idx < N; ++idx) {
        int s = order[idx];

        for (int js = 0; js < K; ++js) {
            b[js] = unaryCosts[s][js];
        }

        for (const auto& item : inc[s]) {
            int e = item.edgeId;
            int u = item.neighbor;
            int endpointFlag = item.endpointFlag;
            const double* C = pairCosts[e].data();

            if (pos0[u] < pos0[s]) {
                int lu = labels[u];

                if (endpointFlag == 1) {
                    for (int js = 0; js < K; ++js) {
                        b[js] += C[js * K + lu];
                    }
                } else {
                    for (int js = 0; js < K; ++js) {
                        b[js] += C[lu * K + js];
                    }
                }
            } else {
                if (endpointFlag == 1) {
                    for (int js = 0; js < K; ++js) b[js] += msg21[e * K + js];
                } else {
                    for (int js = 0; js < K; ++js) b[js] += msg12[e * K + js];
                }
            }
        }

        labels[s] = static_cast<int>(
            std::min_element(b.begin(), b.end()) - b.begin()
            );
    }

    return labels;
}

double TRWS::computeEnergy(const std::vector<int>& labels) const {
    double energy = 0.0;

    for (int i = 0; i < N; ++i) {
        energy += unaryCosts[i][labels[i]];
    }

    for (int e = 0; e < M; ++e) {
        int a = edges[e].first;
        int b = edges[e].second;
        energy += pairCosts[e][labels[a] * K + labels[b]];
    }

    return energy;
}
