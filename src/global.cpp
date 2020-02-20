#include "global.hpp"

#include <iostream>
#include <string>

ProofOutput GlobalVariables::proofOutput;

boost::filesystem::path getProofFile() {
    return (boost::filesystem::temp_directory_path() / boost::filesystem::unique_path()).replace_extension(".txt");
}
