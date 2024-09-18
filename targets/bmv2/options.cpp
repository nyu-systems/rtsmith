#include "backends/p4tools/modules/p4rtsmith/targets/bmv2/options.h"

#include <iostream>  // TODO(zzmic): Remove this eventually.
#include <random>

#include "backends/p4tools/common/compiler/context.h"
#include "backends/p4tools/common/lib/logging.h"
#include "backends/p4tools/common/lib/util.h"
#include "backends/p4tools/common/options.h"
#include "backends/p4tools/modules/p4rtsmith/toolname.h"

namespace P4::P4Tools::RTSmith::V1Model {

Bmv2V1ModelRtSmithOptions &Bmv2V1ModelRtSmithOptions::get() {
    return P4Tools::CompileContext<Bmv2V1ModelRtSmithOptions>::get().options();
}

const std::set<std::string> K_SUPPORTED_CONTROL_PLANES = {"P4RUNTIME", "BFRUNTIME"};

Bmv2V1ModelRtSmithOptions::Bmv2V1ModelRtSmithOptions() : RtSmithOptions() {
    registerOption(
        "--print-to-stdout", nullptr,
        [this](const char *) {
            _printToStdout = true;
            return true;
        },
        "Whether to write the generated config to a file or to stdout.");
    registerOption(
        "--output-dir", "outputDir",
        [this](const char *arg) {
            _outputDir = std::filesystem::path(arg);
            return true;
        },
        "The path where config file(s) are being emitted.");
    registerOption(
        "--config-name", "configName",
        [this](const char *arg) {
            _configName = arg;
            return true;
        },
        "The base name of the config files. Optional.");
    registerOption(
        "--user-p4info", "filePath",
        [this](const char *arg) {
            _userP4Info = arg;
            if (!std::filesystem::exists(_userP4Info.value())) {
                ::P4::error("%1% does not exist. Please provide a valid file path.",
                            _userP4Info.value().c_str());
                return false;
            }
            return true;
        },
        "Use user-provided P4Runtime control plane API description (P4Info).");
    registerOption(
        "--generate-p4info", "filePath",
        [this](const char *arg) {
            _p4InfoFilePath = arg;
            if (_p4InfoFilePath.value().extension() != ".txtpb") {
                ::P4::error("%1% must have a .txtpb extension.", _p4InfoFilePath.value().c_str());
                return false;
            }
            return true;
        },
        "Write the P4Runtime control plane API description (P4Info) to the specified .txtpb file.");
    registerOption(
        "--control-plane", "controlPlaneApi",
        [this](const char *arg) {
            _controlPlaneApi = arg;
            transform(_controlPlaneApi.begin(), _controlPlaneApi.end(), _controlPlaneApi.begin(),
                      ::toupper);
            if (K_SUPPORTED_CONTROL_PLANES.find(_controlPlaneApi) ==
                K_SUPPORTED_CONTROL_PLANES.end()) {
                ::P4::error(
                    "Test back end %1% not implemented for this target. Supported back ends are "
                    "%2%.",
                    _controlPlaneApi, Utils::containerToString(K_SUPPORTED_CONTROL_PLANES));
                return false;
            }
            return true;
        },
        "Specifies the control plane API to use. Defaults to P4Rtuntime.");
    registerOption(
        "--random-seed", nullptr,
        [this](const char *) {
            if (seed.has_value()) {
                error("Seed has already been set to %1%.", *seed);
                return false;
            }
            // No seed provided, we generate our own.
            std::random_device r;
            seed = r();
            Utils::setRandomSeed(*seed);
            printInfo("Using randomly generated seed %1%", *seed);
            return true;
        },
        "Use a random seed.");
    registerOption(
        "--toml", "filePath",
        [this](const char *arg) {
            _fuzzerConfigPath = arg;
            if (_fuzzerConfigPath.value().extension() != ".toml") {
                ::P4::error("%1% must have a .toml extension.", _fuzzerConfigPath.value().c_str());
                return false;
            }
            // Override the default fuzzer configurations with the configurations from the TOML
            // file.
            _bmv2V1ModelFuzzerConfig.override_fuzzer_configs(_fuzzerConfigPath.value().c_str());

            // TODO(zzmic): Delete the following debugging prints eventually.
            std::cout << "Fuzzer configurations have been set in _bmv2V1ModelFuzzerConfig."
                      << "\n";
            std::cout << "Overriden maxEntriesPerTable: "
                      << _bmv2V1ModelFuzzerConfig.getMaxEntriesPerTable() << "\n";
            std::cout << "Overriden maxTables: " << _bmv2V1ModelFuzzerConfig.getMaxTables() << "\n";
            std::cout << "Overriden tablesToSkip:\n";
            for (const auto &table : _bmv2V1ModelFuzzerConfig.getTablesToSkip()) {
                std::cout << table << "\n";
            }
            std::cout << "Overriden matchKindOpt: " << _bmv2V1ModelFuzzerConfig.getMatchKindOpt()
                      << "\n";

            return true;
        },
        "Set the fuzzer configurations using the TOML file specified by the file path");
}

}  // namespace P4::P4Tools::RTSmith::V1Model