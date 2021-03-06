
#include <cassert>
#include <fstream>
#include <vector>

#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>

#include "common/arg_parser.h"
#include "common/benchmark.h"
#include "common/comparison.h"
#include "common/instrumentation.h"

void usage(char* name) {
    using std::cout;
    using std::endl;
    cout << "Usage: " << name << " <options>" << endl << endl
         << "Options:" << endl
         << "-i <filename> input serialization filename (default: data.txt)" << endl
         << "-c <double>   cutoff, at which difference ratio to stop printing (deafult: 1.01)" << endl
         << "-m <int>      maximum number of differences to print (default: 10)" << endl
         << "-b <int>      which contender to compare to the others (default: 0)" << endl;
    exit(0);
}

int main(int argc, char** argv) {
    common::arg_parser args(argc, argv);
    if (args.is_set("h") || args.is_set("-help")) usage(argv[0]);
    const int max_results = args.get<int>("m", 10),
              base_contender = args.get<int>("b", 0);
    const double cutoff = args.get<double>("c", 1.01);
    const std::string &filename = args.get<std::string>("i", "data.txt");

    std::vector<std::vector<common::benchmark_result_aggregate>> results;

    // Serialize results
    std::ifstream ifs(filename);
    if (!ifs.good() || !ifs.is_open()) {
        std::cerr << "Can't open file: " << filename << std::endl;
        exit(1);
    }
    boost::archive::text_iarchive ia(ifs);
    ia >> results;

    common::comparison comparison(results, base_contender);
    comparison.compare();
    comparison.print(std::cout, cutoff, max_results);
}
