#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"
#include <vector>
#include <fstream>
#include <string>

using namespace llvm;

#include <algorithm>
#include <cctype>
#include <locale>
#include <string>

// Trim from start (in place)
static inline void ltrim(std::string &s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch)
                                    { return !std::isspace(ch); }));
}

// Trim from end (in place)
static inline void rtrim(std::string &s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch)
                         { return !std::isspace(ch); })
                .base(),
            s.end());
}

// Trim from both ends (in place)
static inline void trim(std::string &s)
{
    ltrim(s);
    rtrim(s);
}

namespace
{

    cl::opt<std::string> StringsFilePath("strings", cl::desc("Path to string file (one string per line)"), cl::init("strings.txt"));
    cl::opt<std::string> LabelsFilePath("labels", cl::desc("Path to label file (one label per line)"), cl::init("labels.txt"));
    cl::opt<unsigned int> SeedValue("seed", cl::desc("Seed for pseudorandom number generator"), cl::init(42));

    // Simple pseudorandom number generator
    unsigned int my_rand_state;
    void my_srand(unsigned int seed)
    {
        my_rand_state = seed;
    }

    unsigned int my_rand()
    {
        my_rand_state = my_rand_state * 1103515245 + 12345;
        return (my_rand_state / 65536) % 32768;
    }

    struct JobSecurityPass : public PassInfoMixin<JobSecurityPass>
    {
        PreservedAnalyses run(Module &M, ModuleAnalysisManager &)
        {
            LLVMContext &Context = M.getContext();

            // Initialize pseudorandom number generator with seed
            my_srand(SeedValue);
            errs() << "Using seed value: " << SeedValue << "\n";

            // Load strings from file
            std::vector<std::string> strings;
            std::ifstream stringFile(StringsFilePath);
            if (stringFile.is_open())
            {
                std::string line;
                while (std::getline(stringFile, line))
                {
                    trim(line);
                    if (!line.empty())
                        strings.push_back(line);
                }
                stringFile.close();
                errs() << "Loaded " << strings.size() << " strings from file: " << StringsFilePath << "\n";
            }
            else
            {
                errs() << "ERROR: Unable to open string file, exiting.\n";
                exit(1);
            }

            // Load labels from file
            std::vector<std::string> labels;
            std::ifstream labelFile(LabelsFilePath);
            if (labelFile.is_open())
            {
                std::string line;
                while (std::getline(labelFile, line))
                {
                    if (!line.empty())
                        labels.push_back(line);
                }
                labelFile.close();
                errs() << "Loaded " << labels.size() << " labels from file: " << LabelsFilePath << "\n";
            }
            else
            {
                errs() << "ERROR: Unable to open label file, exiting.\n";
                exit(1);
            }

            if (strings.empty())
            {
                errs() << "ERROR: No strings found, exiting.\n";
                exit(1);
            }

            if (labels.empty())
            {
                errs() << "ERROR: No labels found, exiting.\n";
                exit(1);
            }

            auto getRandomLabel = [&labels]() -> std::string
            {
                return labels[my_rand() % labels.size()];
            };

            std::vector<Constant *> StringPtrs;
            Constant *Zero = ConstantInt::get(Type::getInt32Ty(Context), 0);
            for (const auto &str : strings)
            {
                Constant *Str = ConstantDataArray::getString(Context, str, true);
                GlobalVariable *StrVar = new GlobalVariable(
                    M, Str->getType(), true, GlobalValue::PrivateLinkage, Str);
                Constant *StrPtr = ConstantExpr::getInBoundsGetElementPtr(
                    Str->getType(), StrVar, Zero);
                StringPtrs.push_back(StrPtr);
            }

            auto getRandomString = [&StringPtrs]() -> Constant *
            {
                return StringPtrs[my_rand() % StringPtrs.size()];
            };

            PointerType *VoidPtrTy = PointerType::get(Type::getInt8Ty(Context), 0);
            GlobalVariable *GlobalPtrVar = new GlobalVariable(
                M, VoidPtrTy, false, GlobalValue::InternalLinkage, getRandomString(), getRandomLabel());

            for (Function &F : M)
            {
                int counter = 0;
                for (BasicBlock &BB : F)
                {
                    IRBuilder<> Builder(&BB);
                    for (Instruction &I : BB)
                    {
                        if (counter++ % 5 == 0)
                        {
                            Builder.SetInsertPoint(&I);

                            Value *Ptr = Builder.CreatePointerCast(getRandomString(), Type::getInt8Ty(Context)->getPointerTo());
                            Builder.CreateStore(Ptr, GlobalPtrVar);
                        }
                    }
                }
            }

            int  labelCounter= 0;
            for (Function &F : M)
            {
                for (BasicBlock &BB : F)
                {
                    std::string label = labels[labelCounter % labels.size()];
                    labelCounter++;
                    Twine LabelName = Twine(label) + Twine("_") + Twine(labelCounter);
                    BB.setName(LabelName);
                }
            }

            return PreservedAnalyses::none();
        }
    };

    llvm::PassPluginLibraryInfo getJobSecurityPassPluginInfo()
    {
        return {LLVM_PLUGIN_API_VERSION, "job-security", LLVM_VERSION_STRING,
                [](PassBuilder &PB)
                {
                    PB.registerPipelineParsingCallback(
                        [](StringRef Name, ModulePassManager &MPM, ArrayRef<PassBuilder::PipelineElement>)
                        {
                            if (Name == "job-security")
                            {
                                MPM.addPass(JobSecurityPass());
                                return true;
                            }
                            return false;
                        });
                }};
    }

    extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo()
    {
        return getJobSecurityPassPluginInfo();
    }

} // namespace
