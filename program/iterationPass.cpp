#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/LoopIterator.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/SimplifyIndVar.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Verifier.h"

#include "llvm/IR/LegacyPassManager.h"

#define debug_level 2

using namespace llvm;
using namespace std;


namespace {

    char strs[12][20] = {"scConstant", "scTruncate", "scZeroExtend", "scSignExtend", "scAddExpr",
                         "scMulExpr",
                         "scUDivExpr", "scAddRecExpr", "scUMaxExpr", "scSMaxExpr",
                         "scUnknown", "scCouldNotCompute"};

    //LIC pass
    struct LoopIter : public FunctionPass {
        static char ID;



        void prindDebugLevel(const char *str,const char *val){
            if (debug_level==1){
                errs() << str << ":" << val << ", ";
            }
            else{
                errs() << val << ", ";
            }
        }

        char* concatStrs(char* Loc, const char *t1){
            char *t2;
            if(Loc==NULL){
                Loc = (char *)malloc(strlen(t1) + 1);
                strcpy(Loc,t1);
            }
            else{
                t2 = Loc;
                Loc = (char *)malloc(strlen(t2) + strlen(t1) + 3);
                strcpy(Loc,t2);
                Loc = strcat(Loc,"; ");
                Loc = strcat(Loc,t1);
            }
            return Loc;
        }

#if 0
        void findRangeOperands(const SCEV *UB){
            if(UB->getSCEVType()==4){
                const SCEVNAryExpr *SAE = dyn_cast<SCEVNAryExpr>(UB);
                errs() << "Num of Ops:" << SAE->getNumOperands() << "\n";
                for (auto it = SAE->op_begin(); it != SAE->op_end(); ++it) {
                    const SCEV *operand=*it;
                    errs() << "operand SCEV Type:" << strs[operand->getSCEVType()] << "\n";
//                            if(operand->getSCEVType()==9){
//                                const SCEVSMaxExpr *oper = dyn_cast<SCEVSMaxExpr>(operand);
//                                errs() << "Max:" << SE.getSignedRangeMax(operand) << "\n";
//                            }
                }
            }
        }
#endif
        LoopIter() : FunctionPass(ID) {}

        void getAnalysisUsage(AnalysisUsage &AU) const override {

            AU.addRequired<DominatorTreeWrapperPass>();
            AU.addPreserved<DominatorTreeWrapperPass>();
            AU.addRequired<LoopInfoWrapperPass>();
            AU.addPreserved<LoopInfoWrapperPass>();
            AU.addRequired<ScalarEvolutionWrapperPass>();
            AU.addPreserved<ScalarEvolutionWrapperPass>();

            AU.addRequired<DependenceAnalysisWrapperPass>();
            AU.addPreserved<DependenceAnalysisWrapperPass>();

            AU.setPreservesAll();
        }

        virtual bool runOnFunction(Function &F) {
            int counts = 0;
            int countsFormCond = 0;
            int countsBounded = 0;
            int nestedLoops = 0;
            int countInductive = 0;
            int countpossibleInfiniteLoops = 0;
            char* Loc=NULL;
            LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
            ScalarEvolution &SE = getAnalysis<ScalarEvolutionWrapperPass>().getSE();
            //DependenceInfo &depinfo = getAnalysis<DependenceAnalysisWrapperPass>().getDI();

            int countLoop = 0;
            Module *newModule1 = F.getParent();

            //Iteration on loops
            for (LoopInfo::iterator i = LI.begin(), e = LI.end(); i != e; ++i) {
                Loop *L = *i;
                bool finiteLoop=false;
                counts++;

                int subloop = 1;
                countLoop++;
                if (SE.hasLoopInvariantBackedgeTakenCount(L)) { //Any loop condition that can be formulated goes into this branch
                    finiteLoop = true;
                    countsFormCond++;
                    const SCEV *UB = SE.getBackedgeTakenCount(L);
                    UB = SE.getTruncateOrZeroExtend(UB, UB->getType());
                    const SCEVConstant *SC = dyn_cast<SCEVConstant>(UB);
                    const SCEV *UB2 = SE.getMaxBackedgeTakenCount(L);

                    if(debug_level==2) { //loop condition formula
                        UB->dump();
                    }

                    APInt vM = SE.getUnsignedRangeMax(UB);

                    if(debug_level==2){
                        errs() << "SCEV Type:" << strs[UB->getSCEVType()] << "\n";
                    }

                    if (SC) { //the upper bound of the loop
                        countsBounded++;
                        APInt vv = SC->getAPInt();
                        if(debug_level==2) {
                            errs() << "Bound: "  << vv << "\n";
                        }
                    }
                }

                PHINode *IndVar = L->getCanonicalInductionVariable();
                if (IndVar) { //whether there is an induction variable
                    finiteLoop = true;
                    countInductive++;
                    if (debug_level==2){
                        errs() << "Induction var:";
                        IndVar->dump();
                    }
                }
                if (!finiteLoop){ //if neither inductive nor formulated, then can be infinite
                    countpossibleInfiniteLoops++;
                }

                int depth = L->getLoopDepth();
                if (depth>1){
                    nestedLoops++;
                }

                llvm::DebugLoc DLoc = L->getStartLoc();
                if(DLoc){
                    const char * t1= std::to_string(DLoc.getLine()).c_str();
                    Loc = concatStrs(Loc,t1); //probably a better approach is to increment the same LoC instead of listing it many times
                }
                else{
                    Loc = concatStrs(Loc,"NA");
                }

                if (debug_level==2){
                    L->dumpVerbose();
                }
            }

            if(debug_level){
                errs() << "-----------Function name: " << F.getName() << "-----------\n";
            }
            else{
                errs()  << F.getName() << ", ";
            }
            prindDebugLevel("Infinite loops",std::to_string(countpossibleInfiniteLoops).c_str());
            prindDebugLevel("Bounded loops",std::to_string(countsBounded).c_str());
            prindDebugLevel("Formulated loop condition",std::to_string(countsFormCond).c_str());
            prindDebugLevel("Nested loops",std::to_string(nestedLoops).c_str());//the depth of the loops
            prindDebugLevel("Inductive loops",std::to_string(countInductive).c_str());
            prindDebugLevel("loops in total",std::to_string(counts).c_str());
            if(Loc){
                prindDebugLevel("LoC",Loc);
            }
            else{
                prindDebugLevel("LoC","NA");
            }

            errs() << newModule1->getName().str(); //filename

            if(debug_level){
                errs() << "\n-----------------------------------------\n";
            }
            else{
                errs() << "\n";
            }

//            llvm::DebugLoc DLoc = (*LI.begin())->getStartLoc();
//            std::string dbgInfo;
//            llvm::raw_string_ostream rso(dbgInfo);
//            DLoc.print(rso);
//            std::string dbgStr = rso.str();
//            errs()<<dbgStr;

            return false;
        }
    };

}


char LoopIter::ID = 0;

//    INITIALIZE_PASS_BEGIN(LoopIter, "da",
//                          "Dependence Analysis", true, true)
//        INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
//        INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass)
//        INITIALIZE_PASS_DEPENDENCY(ScalarEvolutionWrapperPass)
//    INITIALIZE_PASS_END(LoopIter, "da", "Dependence Analysis",
//                        true, true)

static RegisterPass<LoopIter> X("LoopCount", "Counts number of loops per program", false, false);
