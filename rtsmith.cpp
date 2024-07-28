#include "backends/p4tools/modules/p4rtsmith/rtsmith.h"

#include <google/protobuf/text_format.h>

#include <cstdlib>
#include <filesystem>

#include "backends/p4tools/common/compiler/compiler_result.h"
#include "backends/p4tools/common/lib/logging.h"
#include "backends/p4tools/modules/p4rtsmith/core/target.h"
#include "backends/p4tools/modules/p4rtsmith/core/util.h"
#include "backends/p4tools/modules/p4rtsmith/register.h"
#include "backends/p4tools/modules/p4rtsmith/toolname.h"
#include "control-plane/p4RuntimeSerializer.h"
#include "lib/error.h"
#include "lib/nullstream.h"

namespace P4Tools::RTSmith {

void RtSmith::registerTarget() {
    // Register all available compiler targets.
    // These are discovered by CMAKE, which fills out the register.h.in file.
    registerRtSmithTargets();
}

namespace {

std::optional<RtSmithResult> runRtSmith(const CompilerResult &rtSmithResult,
                                        const RtSmithOptions &rtSmithOptions) {
    const auto *programInfo = RtSmithTarget::produceProgramInfo(rtSmithResult);
    if (programInfo == nullptr) {
        ::error("Program not supported by target device and architecture.");
        return std::nullopt;
    }
    if (::errorCount() > 0) {
        ::error("P4RuntimeSmith: Encountered errors during preprocessing. Exiting");
        return std::nullopt;
    }

    auto p4RuntimeApi = programInfo->getP4RuntimeApi();
    // printInfo("Inferred API:\n%1%", p4RuntimeApi.p4Info->DebugString());

    if (rtSmithOptions.p4InfoFilePath().has_value()) {
        auto *outputFile = openFile(rtSmithOptions.p4InfoFilePath().value(), true);
        if (outputFile == nullptr) {
            return std::nullopt;
        }
        p4RuntimeApi.serializeP4InfoTo(outputFile, P4::P4RuntimeFormat::TEXT_PROTOBUF);
    }

    auto &fuzzer = RtSmithTarget::getFuzzer(*programInfo);

    auto initialConfig = fuzzer.produceInitialConfig();
    auto timeSeriesUpdates = fuzzer.produceUpdateTimeSeries();

    if (rtSmithOptions.printToStdout()) {
        printInfo("Generated initial configuration:");
        for (const auto &writeRequest : initialConfig) {
            printInfo("%1%", writeRequest->DebugString());
        }

        printInfo("Time series updates:");
        for (const auto &[time, writeRequest] : timeSeriesUpdates) {
            printInfo("Time %1%:\n%2%", writeRequest->DebugString());
        }
    }

    auto dirPath = rtSmithOptions.outputDir();
    if (!dirPath.empty()) {
        if (!std::filesystem::exists(dirPath)) {
            if (!std::filesystem::create_directory(dirPath)) {
                ::error("P4RuntimeSmith: Failed to create output directory. Exiting");
                return std::nullopt;
            }
        }

        auto initialConfigPath = dirPath;
        if (rtSmithOptions.configName().has_value()) {
            initialConfigPath = initialConfigPath / rtSmithOptions.configName().value();
            initialConfigPath = initialConfigPath.replace_extension("_update_0");
        } else {
            initialConfigPath = initialConfigPath / "update_0";
        }
        initialConfigPath = initialConfigPath.replace_extension(".txtpb");

        auto *outputFile = openFile(initialConfigPath, true);
        if (outputFile == nullptr) {
            ::error("P4RuntimeSmith: Config file path doesn't exist. Exiting");
            return std::nullopt;
        }

        for (const auto &writeRequest : initialConfig) {
            std::string output;
            google::protobuf::TextFormat::Printer textPrinter;
            textPrinter.SetExpandAny(true);
            if (!textPrinter.PrintToString(*writeRequest.get(), &output)) {
                ::error(ErrorType::ERR_IO, "Failed to serialize protobuf message to text");
                return std::nullopt;
            }

            *outputFile << output;
            if (!outputFile->good()) {
                ::error(ErrorType::ERR_IO, "Failed to write text protobuf message to the output");
                return std::nullopt;
            }
            printInfo("Wrote initial configuration to %1%", initialConfigPath);

            outputFile->flush();
        }
    }

    return {{std::move(initialConfig), std::move(timeSeriesUpdates)}};
}

}  // namespace

int RtSmith::mainImpl(const CompilerResult &compilerResult) {
    // Register all available P4RuntimeSmith targets.
    // These are discovered by CMAKE, which fills out the register.h.in file.
    registerRtSmithTargets();

    const auto &rtSmithOptions = RtSmithOptions::get();
    auto result = runRtSmith(compilerResult, rtSmithOptions);
    return (result.has_value() && ::errorCount() == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

std::optional<RtSmithResult> generateConfigImpl(
    std::optional<std::reference_wrapper<const std::string>> program,
    const RtSmithOptions &rtSmithOptions) {
    // Register supported P4RTSmith targets.
    registerRtSmithTargets();

    P4Tools::Target::init(rtSmithOptions.target.c_str(), rtSmithOptions.arch.c_str());

    CompilerResultOrError compilerResult;
    if (program.has_value()) {
        // Run the compiler to get an IR and invoke the tool.
        ASSIGN_OR_RETURN(
            compilerResult,
            P4Tools::CompilerTarget::runCompiler(rtSmithOptions, TOOL_NAME, program->get()),
            std::nullopt);
    } else {
        RETURN_IF_FALSE_WITH_MESSAGE(!rtSmithOptions.file.empty(), std::nullopt,
                                     ::error("Expected a file input."));
        // Run the compiler to get an IR and invoke the tool.
        ASSIGN_OR_RETURN(compilerResult,
                         P4Tools::CompilerTarget::runCompiler(rtSmithOptions, TOOL_NAME),
                         std::nullopt);
    }

    return std::move(runRtSmith(compilerResult.value(), rtSmithOptions));
}

std::optional<RtSmithResult> RtSmith::generateConfig(const std::string &program,
                                                     const RtSmithOptions &rtSmithOptions) {
    try {
        return generateConfigImpl(program, rtSmithOptions);
    } catch (const std::exception &e) {
        std::cerr << "Internal error: " << e.what() << "\n";
        return std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<RtSmithResult> RtSmith::generateConfig(const RtSmithOptions &rtSmithOptions) {
    try {
        return std::move(generateConfigImpl(std::nullopt, rtSmithOptions));
    } catch (const std::exception &e) {
        std::cerr << "Internal error: " << e.what() << "\n";
        return std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
    return std::nullopt;
}

}  // namespace P4Tools::RTSmith
