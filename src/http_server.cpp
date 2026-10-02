#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <cstdlib>

#include <httplib.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct Offer {
    std::string seller;
    double price;
    double deliveryDays;
    double rating;
    double topsisScore = 0.0;
};

struct RankingResult {
    std::string winner;
    double priceWeight = 0.0;
    double deliveryWeight = 0.0;
    double ratingWeight = 0.0;
    std::vector<Offer> ranking;
};

RankingResult runV9(std::vector<Offer> offers) {

    RankingResult result;

    double minPrice = offers[0].price;
    double maxPrice = offers[0].price;

    double minDelivery = offers[0].deliveryDays;
    double maxDelivery = offers[0].deliveryDays;

    double minRating = offers[0].rating;
    double maxRating = offers[0].rating;

    for (const auto& offer : offers) {

        minPrice = std::min(minPrice, offer.price);
        maxPrice = std::max(maxPrice, offer.price);

        minDelivery = std::min(minDelivery, offer.deliveryDays);
        maxDelivery = std::max(maxDelivery, offer.deliveryDays);

        minRating = std::min(minRating, offer.rating);
        maxRating = std::max(maxRating, offer.rating);
    }

    std::vector<std::vector<double>> normalizedMatrix;

    for (const auto& offer : offers) {

        double normalizedPrice = 1.0;

        if (maxPrice != minPrice) {
            normalizedPrice =
                (maxPrice - offer.price) /
                (maxPrice - minPrice);
        }

        double normalizedDelivery = 1.0;

        if (maxDelivery != minDelivery) {
            normalizedDelivery =
                (maxDelivery - offer.deliveryDays) /
                (maxDelivery - minDelivery);
        }

        double normalizedRating = 1.0;

        if (maxRating != minRating) {
            normalizedRating =
                (offer.rating - minRating) /
                (maxRating - minRating);
        }

        normalizedMatrix.push_back({
            normalizedPrice,
            normalizedDelivery,
            normalizedRating
        });
    }

    const size_t offerCount = normalizedMatrix.size();
    const size_t criteriaCount = 3;

    std::vector<double> diversification(criteriaCount, 0.0);

    const double k =
        1.0 / std::log(static_cast<double>(offerCount));

    for (size_t j = 0; j < criteriaCount; ++j) {

        double columnSum = 0.0;

        for (size_t i = 0; i < offerCount; ++i) {
            columnSum += normalizedMatrix[i][j];
        }

        double entropy = 0.0;

        if (columnSum > 0.0) {

            for (size_t i = 0; i < offerCount; ++i) {

                const double p =
                    normalizedMatrix[i][j] / columnSum;

                if (p > 0.0) {
                    entropy += p * std::log(p);
                }
            }

            entropy *= -k;

        } else {

            entropy = 1.0;
        }

        diversification[j] = 1.0 - entropy;

        if (diversification[j] < 0.0) {
            diversification[j] = 0.0;
        }
    }

    double diversificationTotal = 0.0;

    for (double value : diversification) {
        diversificationTotal += value;
    }

    if (diversificationTotal > 0.0) {

        result.priceWeight =
            diversification[0] / diversificationTotal;

        result.deliveryWeight =
            diversification[1] / diversificationTotal;

        result.ratingWeight =
            diversification[2] / diversificationTotal;

    } else {

        result.priceWeight = 1.0 / 3.0;
        result.deliveryWeight = 1.0 / 3.0;
        result.ratingWeight = 1.0 / 3.0;
    }

    std::vector<std::vector<double>> weightedMatrix;

    for (const auto& row : normalizedMatrix) {

        weightedMatrix.push_back({
            row[0] * result.priceWeight,
            row[1] * result.deliveryWeight,
            row[2] * result.ratingWeight
        });
    }

    double idealBestPrice = weightedMatrix[0][0];
    double idealBestDelivery = weightedMatrix[0][1];
    double idealBestRating = weightedMatrix[0][2];

    double idealWorstPrice = weightedMatrix[0][0];
    double idealWorstDelivery = weightedMatrix[0][1];
    double idealWorstRating = weightedMatrix[0][2];

    for (const auto& row : weightedMatrix) {

        idealBestPrice =
            std::max(idealBestPrice, row[0]);

        idealBestDelivery =
            std::max(idealBestDelivery, row[1]);

        idealBestRating =
            std::max(idealBestRating, row[2]);

        idealWorstPrice =
            std::min(idealWorstPrice, row[0]);

        idealWorstDelivery =
            std::min(idealWorstDelivery, row[1]);

        idealWorstRating =
            std::min(idealWorstRating, row[2]);
    }

    for (size_t i = 0; i < offers.size(); ++i) {

        const auto& row = weightedMatrix[i];

        const double distanceToBest =
            std::sqrt(
                std::pow(row[0] - idealBestPrice, 2) +
                std::pow(row[1] - idealBestDelivery, 2) +
                std::pow(row[2] - idealBestRating, 2)
            );

        const double distanceToWorst =
            std::sqrt(
                std::pow(row[0] - idealWorstPrice, 2) +
                std::pow(row[1] - idealWorstDelivery, 2) +
                std::pow(row[2] - idealWorstRating, 2)
            );

        const double denominator =
            distanceToBest + distanceToWorst;

        if (denominator > 0.0) {
            offers[i].topsisScore =
                distanceToWorst / denominator;
        } else {
            offers[i].topsisScore = 1.0;
        }
    }

    std::stable_sort(
        offers.begin(),
        offers.end(),
        [](const Offer& a, const Offer& b) {
            return a.topsisScore > b.topsisScore;
        }
    );

    result.winner = offers[0].seller;
    result.ranking = offers;

    return result;
}

int listenPortFromEnvironment() {
    const char* raw = std::getenv("PORT");

    if (raw == nullptr || raw[0] == '\0') {
        return 8080;
    }

    char* end = nullptr;
    const long parsed = std::strtol(raw, &end, 10);

    if (end == raw || *end != '\0' || parsed < 1 || parsed > 65535) {
        return -1;
    }

    return static_cast<int>(parsed);
}

int main() {

    const int port = listenPortFromEnvironment();

    if (port < 1) {
        std::cerr << "Invalid PORT value" << std::endl;
        return 1;
    }

    httplib::Server server;

    server.Get("/health",
        [](const httplib::Request&,
           httplib::Response& res) {

            json response = {
                {"status", "ok"},
                {"service", "MerqnetEngine"},
                {"engineVersion", "V9"}
            };

            res.set_content(
                response.dump(),
                "application/json"
            );
        }
    );

    server.Post("/rank",
        [](const httplib::Request& req,
           httplib::Response& res) {

            try {

                json input =
                    json::parse(req.body);

                if (
                    !input.contains("offers") ||
                    !input["offers"].is_array() ||
                    input["offers"].size() < 2
                ) {

                    json error = {
                        {"error", "At least 2 offers are required"}
                    };

                    res.status = 400;

                    res.set_content(
                        error.dump(),
                        "application/json"
                    );

                    return;
                }

                std::vector<Offer> offers;

                for (const auto& item : input["offers"]) {

                    Offer offer;

                    offer.seller =
                        item.at("seller").get<std::string>();

                    offer.price =
                        item.at("price").get<double>();

                    offer.deliveryDays =
                        item.at("deliveryDays").get<double>();

                    offer.rating =
                        item.at("rating").get<double>();

                    offers.push_back(offer);
                }

                RankingResult result =
                    runV9(offers);

                json ranking =
                    json::array();

                int position = 1;

                for (const auto& offer : result.ranking) {

                    ranking.push_back({
                        {"position", position},
                        {"seller", offer.seller},
                        {"price", offer.price},
                        {"deliveryDays", offer.deliveryDays},
                        {"rating", offer.rating},
                        {"score", offer.topsisScore}
                    });

                    ++position;
                }

                json response = {

                    {"engineVersion", "V9"},

                    {"weightMethod", "entropy"},

                    {"autoWeights", {
                        {"price", result.priceWeight},
                        {"delivery", result.deliveryWeight},
                        {"rating", result.ratingWeight}
                    }},

                    {"bestMatch", result.winner},

                    {"ranking", ranking}
                };

                res.set_content(
                    response.dump(2),
                    "application/json"
                );
            }
            catch (const std::exception& e) {

                json error = {
                    {"error", "Invalid request"},
                    {"details", e.what()}
                };

                res.status = 400;

                res.set_content(
                    error.dump(),
                    "application/json"
                );
            }
        }
    );

    std::cout << "MerqnetEngine HTTP server running on 0.0.0.0:"
              << port << std::endl;
    std::cout << "Health: http://localhost:" << port << "/health" << std::endl;
    std::cout << "POST:   http://localhost:" << port << "/rank" << std::endl;

    if (!server.listen("0.0.0.0", port)) {
        std::cerr << "Failed to listen on 0.0.0.0:" << port << std::endl;
        return 1;
    }

    return 0;
}