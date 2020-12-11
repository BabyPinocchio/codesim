#include <iostream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <memory>
#include <unistd.h>
#include <getopt.h>
#include <vector>
#include <queue>
#include <set>
#include <functional>
#include "math.h"

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Parse/ParseAST.h"
#include "clang/Parse/Parser.h"
#include "clang/Tooling/ASTDiff/ASTDiff.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "clang/Lex/Token.h"

using namespace std;
using namespace llvm;
using namespace clang;
using namespace clang::tooling;

// libclang (for parsing ASTs)
// clang (for creating ASTs)
static bool hOption = false;
static bool vOption = false;

// 得到一个 CompilerInstance
unique_ptr<CompilerInstance> getCompilerInstance(const StringRef programName) {
    std::unique_ptr<CompilerInstance> ci(new CompilerInstance);
	
    ci->createDiagnostics(nullptr,false);
    shared_ptr<clang::TargetOptions> targetOptions(new clang::TargetOptions);
    targetOptions->Triple = llvm::sys::getDefaultTargetTriple();
    clang::IntrusiveRefCntPtr<clang::TargetInfo> targetInfoPtr(
            clang::TargetInfo::CreateTargetInfo(ci->getDiagnostics(), targetOptions));
    TargetInfo *ptrTarInfo = TargetInfo::CreateTargetInfo(ci->getDiagnostics(),targetOptions);
	ci->setTarget(ptrTarInfo);
    ci->createFileManager();
	ci->createSourceManager(ci->getFileManager());
	ci->createPreprocessor(TU_Complete);
	ci->createASTContext();

    const char *headerSearchPaths[] = {
        "/usr/include/c++/7.5.0",
        "/usr/lib/llvm-6.0/lib/clang/6.0.0/include",
        "/usr/include/x86_64-linux-gnu/c++/7.5.0",
        "/usr/include/x86_64-linux-gnu", 
        "/usr/lib/llvm-6.0/include",
        "/include",
        "/usr/include",
        "/usr/include/llvm-6.0"
    };

    for (size_t i = 0; i < sizeof(headerSearchPaths)/sizeof(const char *); ++i) {
        ci->getHeaderSearchOpts().AddPath(headerSearchPaths[i], clang::frontend::Angled, false, false);
    }

	// const FileEntry *fileEntry = ci->getFileManager().getFile(programName).get();
    const FileEntry *fileEntry = ci->getFileManager().getFile(programName);
	ci->getSourceManager().setMainFileID(ci->getSourceManager().createFileID(fileEntry, SourceLocation(), SrcMgr::C_User));
	
	ASTConsumer *consumer = new ASTConsumer;
	ci->setASTConsumer(std::unique_ptr<ASTConsumer>(consumer));
	ci->createSema(TU_Complete, nullptr);

	ci->getDiagnostics().setSeverityForAll(clang::diag::Flavor::WarningOrError, clang::diag::Severity::Ignored);
	ci->getDiagnosticClient().BeginSourceFile(ci->getLangOpts(), &ci->getPreprocessor());
	ParseAST(ci->getPreprocessor(), &*consumer, ci->getASTContext());
    ci->getDiagnosticClient().EndSourceFile();
    return ci;
}

vector<Token> getTokensFromProgram(CompilerInstance &ci){
    ci.getPreprocessor().EnterMainSourceFile();
    vector<Token> tokens;
	do {
	  Token token;
	  ci.getPreprocessor().Lex(token);
	  auto srcFileId = ci.getSourceManager().getMainFileID();
	  auto dstFileId = ci.getSourceManager().getFileID(token.getLocation());
	  if (srcFileId == dstFileId)
        tokens.push_back(token);
    } while (tokens.size() == 0 || tokens[tokens.size() - 1].isNot(clang::tok::eof));
    tokens.pop_back();
	ci.getDiagnosticClient().EndSourceFile();
    return tokens;
}

int getLCS(vector<clang::Token> &a, vector<clang::Token> &b) {
    int aLength = a.size();
    int bLength = b.size();
    int **dp = new int*[aLength + 1];
    for(int i=0; i<aLength+1; i++){
        dp[i] = new int[bLength+1];
    }
    for(int i=0; i<aLength+1; i++){
        for(int j=0; j<bLength+1; j++){
            dp[i][j] = 0;
        }
    }
    for (int i = 1; i <= aLength; i++) {
        for (int j = 1; j <= bLength; j++) {
            if (a[i - 1].getName() == b[j - 1].getName()) {
                dp[i][j] = dp[i - 1][j - 1] + 1;
            } else {
                dp[i][j] = max(dp[i][j - 1], dp[i - 1][j]);
            }
        }
    }
    return dp[aLength][bLength];
}

// 采用Longest Common Subsequence计算相似度
double getLCSSimilarity(vector<clang::Token> &a, vector<Token> &b){
    double lcs = getLCS(a, b);
    double tokenSim = (2.0 * lcs) / (a.size() + b.size());
    if(vOption){
        cout << "Longest Common Subsequence is " << lcs << "." << endl; 
        cout << "Total size is " << a.size() + b.size() << "." << endl;
    }
    return tokenSim;
}

int getASTSize(diff::SyntaxTree &tree){
    diff::NodeId rootId = tree.getRootId();
    int treeSize = 1;
    const diff::Node &root = tree.getNode(rootId);
    queue<diff::NodeId> q;
    q.push(rootId);
    while (!q.empty()) { 
        const diff::Node &node = tree.getNode(q.front());
        q.pop();
        for(diff::NodeId nodeId: node.Children){
            q.push(nodeId);
            treeSize += 1;
        }
    }     
    return treeSize;
}

double getASTSimilarity(diff::ASTDiff &astDiff, diff::SyntaxTree &sourceTree, diff::SyntaxTree &compareTree){
    double editDistance = 0.0;
    
    for(diff::NodeId id : compareTree){
        const diff::Node &node = compareTree.getNode(id);
        if(node.Change==diff::None){
            editDistance += 0;
        } else if(node.Change==diff::Delete){
            editDistance += 0;
        } else if(node.Change==diff::Update){
            editDistance += 1;
        } else if(node.Change==diff::Insert){
            editDistance += 1;
        } else if(node.Change==diff::Move){
            editDistance += 1;
        } else if(node.Change==diff::UpdateMove){
            editDistance += 2;
        }
    }
    // invalid node
    for(diff::NodeId src : sourceTree){
		if(!astDiff.getMapped(sourceTree,src).isValid())
			editDistance += 1; 
	}
    double ASTSize = getASTSize(sourceTree)+getASTSize(compareTree);
    double similarity = 1 - (editDistance/ASTSize);
    if(vOption){
        cout << "Edit Distance is " << editDistance << "." << endl; 
        cout << "Total AST size is " << ASTSize << "." <<endl;
    }
    return similarity;
}

vector<long> kgrams(vector<clang::Token> &tokens, int k){
    int len = tokens.size();
    vector<long> result;
    std::hash<string> hasher;
    if(len<k){
        string kTokStr = "";
        for(int i=0;i<len;i++){
            kTokStr += tokens[i].getName();
        }
        result.push_back(hasher(kTokStr));
    }else{
        for(int i=0;i<=len-k;i++){
            string kTokStr = "";
            for(int j=i;j<i+k;j++){
                kTokStr += tokens[j].getName();
            }
            result.push_back(hasher(kTokStr));
        }
    }
    return result;
}

vector<long> fingerprints(vector<long> hashSeq, int winSize){
    int len = hashSeq.size();
    vector<long> fps;
    set<int> indexSet;
    if(len<winSize){
        long minVal = hashSeq[0];
        for(int i=0;i<len;i++){
            if(hashSeq[i]<minVal){
                minVal=hashSeq[i];
            }
        }
        fps.push_back(minVal);
    }
    for(int i=0;i<=len-winSize;i++){
        long minVal = hashSeq[i];
        int index = i;
        for(int j=i;j<i+winSize;j++){
            if(hashSeq[j]<minVal){
                minVal = hashSeq[j];
                index = j;
            }
        }
        indexSet.insert(index);
    }
    for(int s: indexSet){
        fps.push_back(hashSeq[s]);
    }
    return fps;
}

double getWinnowSimilarity(vector<clang::Token> &a, vector<clang::Token> &b){
    int k = 5;
    vector<long> aKgrams = kgrams(a, k);
    vector<long> bKgrams = kgrams(b, k);
    vector<long> aFps = fingerprints(aKgrams,4);
    vector<long> bFps = fingerprints(bKgrams,4);
    int simCount = 0;
    for(long j: bFps){
        for(long i: aFps){
            if(i==j){
                simCount++;
                break;
            }
        }
    } 
    if(vOption){
        cout<<"Similarity Tokens Count: "<<simCount<<endl;
        cout<<"Destination File Size: "<<bFps.size()<<endl;
    }
    return simCount/(double)bFps.size();
}

pair<pair<string, string>, string> analyseCommandLines(int argc, char *argv[]) {
    string UsageMsg = "Usage: codesim [-v|--verbose] [-h|--help] [-t|--type] lcs|editdst|winnow code1 code2";
    int option;
    pair<pair<string, string>, string> p;
    static struct option long_options[] = {
        {"verbose", no_argument, NULL, 'v'},
        {"help", no_argument, NULL, 'h'},
        {"type", required_argument, NULL, 't'}
    };
    const char *optstring = "hvt:"; 
    int option_index = 0; 
    string type = "winnow";
    while ((option = getopt_long(argc, argv, optstring, long_options, &option_index)) != -1) {
        switch(option){
            case 'v':
                vOption = true;
                break;
            case 'h':
                hOption = true;
                cout << UsageMsg << endl;
                exit(1);
            case 't':
                type = optarg;
                break;
            default:
                break;
        } 
    }
    pair<string, string> fileCommand;
    if (argc == optind + 2) {
	  fileCommand.first = string(argv[optind]);
	  fileCommand.second = string(argv[optind + 1]);
	} else {
        cout << UsageMsg << endl;
        exit(-1);
    }
    p.first = fileCommand;
    p.second = type;
    return p;
}

double LCS(unique_ptr<CompilerInstance> &srcCI, unique_ptr<CompilerInstance> &dstCI) {
    vector<Token> srcTokens = getTokensFromProgram(*srcCI);
    vector<Token> dstTokens = getTokensFromProgram(*dstCI);
    return getLCSSimilarity(srcTokens, dstTokens);
}

double AST(unique_ptr<CompilerInstance> &srcCI, unique_ptr<CompilerInstance> &dstCI) {
    diff::ComparisonOptions opt; 
    diff::SyntaxTree srcTree(srcCI->getASTContext());
    diff::SyntaxTree dstTree(dstCI->getASTContext());
    diff::ASTDiff AstDiff(srcTree, dstTree, opt);
    return getASTSimilarity(AstDiff, srcTree, dstTree);
}

double Winnow(unique_ptr<CompilerInstance> &srcCI, unique_ptr<CompilerInstance> &dstCI) {
    vector<Token> srcTokens = getTokensFromProgram(*srcCI);
    vector<Token> dstTokens = getTokensFromProgram(*dstCI);
    return getWinnowSimilarity(srcTokens, dstTokens);
}

int main(int argc, char *argv[]){
    pair<pair<string, string>, string> ret = analyseCommandLines(argc, argv);
    pair<string, string> fileArgs = ret.first;
    string srcProgramName = fileArgs.first;
    string dstProgramName = fileArgs.second;
    string type = ret.second;
    unique_ptr<CompilerInstance> srcCI = getCompilerInstance(srcProgramName);
    unique_ptr<CompilerInstance> dstCI = getCompilerInstance(dstProgramName);
    double similarity = 0.0;
    if(type=="lcs"){
	    similarity = LCS(srcCI, dstCI);
    } else if(type == "editdst") {
	    similarity = AST(srcCI, dstCI);
    } else if(type == "winnow") {
	    similarity = Winnow(srcCI, dstCI);
    } else {
        printf("There is something wrong.");
        exit(-1);
    }
    printf("%.4f\n", 100.0 * similarity);
    
    return 0;
}
