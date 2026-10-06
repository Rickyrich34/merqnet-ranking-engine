#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <map>

#include <nlohmann/json.hpp>

using json = nlohmann::json;


// =========================================
// OFFER STRUCT
// =========================================

struct Offer {
    std::string seller;
    double price;
    double deliveryDays;
    double rating;
    double topsisScore = 0.0;
};


// =========================================
// TEST RESULT STRUCT
// =========================================

struct TestResult {
    int datasetNumber = 0;

    std::string winner;

    double winnerScore = 0.0;

    double priceWeight = 0.0;
    double deliveryWeight = 0.0;
    double ratingWeight = 0.0;
};


// =========================================
// LOAD OFFERS
// =========================================

bool loadOffers(
    const std::string& filePath,
    std::vector<Offer>& offers
) {
    offers.clear();

    std::ifstream file(filePath);

    if (!file.is_open()) {
        return false;
    }

    try {
        json data;

        file >> data;

        if (
            !data.contains("offers") ||
            !data["offers"].is_array()
        ) {
            return false;
        }

        for (const auto& item : data["offers"]) {

            Offer offer;

            offer.seller =
                item.at("seller").get<std::string>();

            offer.price =
                item.at("price").get<double>();

            if (
                item.contains("landedSubtotal") &&
                item["landedSubtotal"].is_number()
            ) {
                offer.price =
                    item["landedSubtotal"].get<double>();
            }

            offer.deliveryDays =
                item.at("deliveryDays").get<double>();

            offer.rating =
                item.at("rating").get<double>();

            offers.push_back(offer);
        }
    }
    catch (...) {
        return false;
    }

    return offers.size() >= 2;
}


// =========================================
// RUN V9 ENGINE
// =========================================

TestResult runV9(
    int datasetNumber,
    std::vector<Offer> offers
) {
    TestResult result;

    result.datasetNumber =
        datasetNumber;


    // =========================================
    // MIN / MAX
    // =========================================

    double minPrice =
        offers[0].price;

    double maxPrice =
        offers[0].price;


    double minDelivery =
        offers[0].deliveryDays;

    double maxDelivery =
        offers[0].deliveryDays;


    double minRating =
        offers[0].rating;

    double maxRating =
        offers[0].rating;


    for (const auto& offer : offers) {

        minPrice =
            std::min(
                minPrice,
                offer.price
            );

        maxPrice =
            std::max(
                maxPrice,
                offer.price
            );


        minDelivery =
            std::min(
                minDelivery,
                offer.deliveryDays
            );

        maxDelivery =
            std::max(
                maxDelivery,
                offer.deliveryDays
            );


        minRating =
            std::min(
                minRating,
                offer.rating
            );

        maxRating =
            std::max(
                maxRating,
                offer.rating
            );
    }


    // =========================================
    // MIN-MAX NORMALIZATION
    // =========================================

    std::vector<std::vector<double>>
        normalizedMatrix;


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


    // =========================================
    // ENTROPY AUTO-WEIGHTING
    // =========================================

    const size_t offerCount =
        normalizedMatrix.size();

    const size_t criteriaCount = 3;


    std::vector<double>
        diversification(
            criteriaCount,
            0.0
        );


    const double k =
        1.0 /
        std::log(
            static_cast<double>(
                offerCount
            )
        );


    for (
        size_t j = 0;
        j < criteriaCount;
        ++j
    ) {

        double columnSum = 0.0;


        for (
            size_t i = 0;
            i < offerCount;
            ++i
        ) {

            columnSum +=
                normalizedMatrix[i][j];
        }


        double entropy = 0.0;


        if (columnSum > 0.0) {

            for (
                size_t i = 0;
                i < offerCount;
                ++i
            ) {

                const double p =
                    normalizedMatrix[i][j] /
                    columnSum;


                if (p > 0.0) {

                    entropy +=
                        p * std::log(p);
                }
            }


            entropy *= -k;

        } else {

            entropy = 1.0;
        }


        diversification[j] =
            1.0 - entropy;


        if (
            diversification[j] < 0.0
        ) {
            diversification[j] = 0.0;
        }
    }


    // =========================================
    // AUTO WEIGHTS
    // =========================================

    double diversificationTotal = 0.0;


    for (
        double value :
        diversification
    ) {
        diversificationTotal += value;
    }


    double weightPrice = 0.0;
    double weightDelivery = 0.0;
    double weightRating = 0.0;


    if (
        diversificationTotal > 0.0
    ) {

        weightPrice =
            diversification[0] /
            diversificationTotal;

        weightDelivery =
            diversification[1] /
            diversificationTotal;

        weightRating =
            diversification[2] /
            diversificationTotal;

    } else {

        weightPrice = 1.0 / 3.0;
        weightDelivery = 1.0 / 3.0;
        weightRating = 1.0 / 3.0;
    }


    result.priceWeight =
        weightPrice;

    result.deliveryWeight =
        weightDelivery;

    result.ratingWeight =
        weightRating;


    // =========================================
    // WEIGHTED MATRIX
    // =========================================

    std::vector<std::vector<double>>
        weightedMatrix;


    for (
        const auto& row :
        normalizedMatrix
    ) {

        weightedMatrix.push_back({

            row[0] * weightPrice,

            row[1] * weightDelivery,

            row[2] * weightRating
        });
    }


    // =========================================
    // IDEAL BEST / WORST
    // =========================================

    double idealBestPrice =
        weightedMatrix[0][0];

    double idealBestDelivery =
        weightedMatrix[0][1];

    double idealBestRating =
        weightedMatrix[0][2];


    double idealWorstPrice =
        weightedMatrix[0][0];

    double idealWorstDelivery =
        weightedMatrix[0][1];

    double idealWorstRating =
        weightedMatrix[0][2];


    for (
        const auto& row :
        weightedMatrix
    ) {

        idealBestPrice =
            std::max(
                idealBestPrice,
                row[0]
            );

        idealBestDelivery =
            std::max(
                idealBestDelivery,
                row[1]
            );

        idealBestRating =
            std::max(
                idealBestRating,
                row[2]
            );


        idealWorstPrice =
            std::min(
                idealWorstPrice,
                row[0]
            );

        idealWorstDelivery =
            std::min(
                idealWorstDelivery,
                row[1]
            );

        idealWorstRating =
            std::min(
                idealWorstRating,
                row[2]
            );
    }


    // =========================================
    // TOPSIS
    // =========================================

    for (
        size_t i = 0;
        i < offers.size();
        ++i
    ) {

        const auto& row =
            weightedMatrix[i];


        const double distanceToBest =
            std::sqrt(

                std::pow(
                    row[0] -
                    idealBestPrice,
                    2
                )

                +

                std::pow(
                    row[1] -
                    idealBestDelivery,
                    2
                )

                +

                std::pow(
                    row[2] -
                    idealBestRating,
                    2
                )
            );


        const double distanceToWorst =
            std::sqrt(

                std::pow(
                    row[0] -
                    idealWorstPrice,
                    2
                )

                +

                std::pow(
                    row[1] -
                    idealWorstDelivery,
                    2
                )

                +

                std::pow(
                    row[2] -
                    idealWorstRating,
                    2
                )
            );


        const double denominator =
            distanceToBest +
            distanceToWorst;


        if (denominator > 0.0) {

            offers[i].topsisScore =
                distanceToWorst /
                denominator;

        } else {

            offers[i].topsisScore = 1.0;
        }
    }


    // =========================================
    // SORT
    // =========================================

    std::sort(
        offers.begin(),
        offers.end(),

        [](const Offer& a,
           const Offer& b) {

            return
                a.topsisScore >
                b.topsisScore;
        }
    );


    result.winner =
        offers[0].seller;

    result.winnerScore =
        offers[0].topsisScore;


    return result;
}


// =========================================
// LOAD EXPECTED RESULTS
// =========================================

bool loadExpectedResults(
    const std::string& filePath,
    std::map<int, std::string>& expected
) {
    std::ifstream file(filePath);

    if (!file.is_open()) {
        return false;
    }


    try {

        json data;

        file >> data;


        if (!data.is_array()) {
            return false;
        }


        for (const auto& item : data) {

            int dataset =
                item.at("dataset").get<int>();

            std::string winner =
                item.at("winner").get<std::string>();

            expected[dataset] =
                winner;
        }
    }
    catch (...) {
        return false;
    }


    return true;
}


// =========================================
// FORMAT DATASET FILE NAME
// =========================================

std::string datasetFileName(
    int number
) {

    std::ostringstream stream;

    stream
        << "..\\data\\tests\\dataset_"
        << std::setw(2)
        << std::setfill('0')
        << number
        << ".json";

    return stream.str();
}


// =========================================
// MAIN
// =========================================

int main() {

    const std::string expectedFile =
        "..\\data\\tests\\expected_results.json";


    std::map<int, std::string>
        expectedResults;


    if (
        !loadExpectedResults(
            expectedFile,
            expectedResults
        )
    ) {

        std::cout << "\n";

        std::cout
            << "ERROR: Could not load:\n";

        std::cout
            << expectedFile
            << "\n";

        return 1;
    }


    // =========================================
    // HEADER
    // =========================================

    std::cout << "\n";

    std::cout
        << "========================================================\n";

    std::cout
        << "       MERQNET V9 - 50 DATASET BATCH TEST\n";

    std::cout
        << "========================================================\n\n";


    int passed = 0;
    int failed = 0;
    int errors = 0;


    std::map<std::string, int>
        winnerCounts;


    // =========================================
    // RUN 50 DATASETS
    // =========================================

    for (
        int datasetNumber = 1;
        datasetNumber <= 50;
        ++datasetNumber
    ) {

        const std::string filePath =
            datasetFileName(
                datasetNumber
            );


        std::vector<Offer> offers;


        if (
            !loadOffers(
                filePath,
                offers
            )
        ) {

            std::cout
                << "Dataset "
                << std::setw(2)
                << std::setfill('0')
                << datasetNumber
                << "   ERROR - Could not read file\n";


            ++errors;

            continue;
        }


        TestResult result =
            runV9(
                datasetNumber,
                offers
            );


        winnerCounts[
            result.winner
        ]++;


        std::string expectedWinner =
            "UNKNOWN";


        if (
            expectedResults.count(
                datasetNumber
            )
        ) {

            expectedWinner =
                expectedResults[
                    datasetNumber
                ];
        }


        const bool pass =
            result.winner ==
            expectedWinner;


        if (pass) {
            ++passed;
        } else {
            ++failed;
        }


        std::cout
            << "Dataset "
            << std::setw(2)
            << std::setfill('0')
            << datasetNumber
            << "   ";


        if (pass) {

            std::cout
                << "PASS";

        } else {

            std::cout
                << "FAIL";
        }


        std::cout
            << "   Winner: "
            << result.winner;


        std::cout
            << "   Expected: "
            << expectedWinner;


        std::cout
            << "   Score: "
            << std::fixed
            << std::setprecision(4)
            << result.winnerScore;


        std::cout
            << "\n";


        std::cout
            << "             Weights -> "
            << "Price "
            << std::setprecision(2)
            << result.priceWeight * 100.0
            << "% | Delivery "
            << result.deliveryWeight * 100.0
            << "% | Rating "
            << result.ratingWeight * 100.0
            << "%\n";
    }


    // =========================================
    // SUMMARY
    // =========================================

    std::cout << "\n";

    std::cout
        << "========================================================\n";

    std::cout
        << "                    TEST SUMMARY\n";

    std::cout
        << "========================================================\n";


    std::cout
        << "Passed: "
        << passed
        << "\n";


    std::cout
        << "Failed: "
        << failed
        << "\n";


    std::cout
        << "File errors: "
        << errors
        << "\n";


    std::cout
        << "Total processed: "
        << passed + failed
        << "\n\n";


    std::cout
        << "WINNER DISTRIBUTION\n";


    std::cout
        << "Seller A: "
        << winnerCounts["Seller A"]
        << "\n";


    std::cout
        << "Seller B: "
        << winnerCounts["Seller B"]
        << "\n";


    std::cout
        << "Seller C: "
        << winnerCounts["Seller C"]
        << "\n\n";


    if (
        passed == 50 &&
        failed == 0 &&
        errors == 0
    ) {

        std::cout
            << "RESULT: ALL 50 TESTS PASSED\n";

    } else {

        std::cout
            << "RESULT: REVIEW FAILURES OR FILE ERRORS\n";
    }


    std::cout
        << "========================================================\n";


    return 0;
}