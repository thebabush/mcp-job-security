LLVM_CONFIG = llvm-config
CLANG = clang
CXX = clang++
OPT = opt

CXXFLAGS = `$(LLVM_CONFIG) --cxxflags` -fPIC -shared -O3
LDFLAGS = `$(LLVM_CONFIG) --ldflags --system-libs --libs all`

PASS_NAME = JobSecurityPass
PASS_SO = $(PASS_NAME).so
SRC = $(PASS_NAME).cpp

all: $(PASS_SO) run

$(PASS_SO): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(PASS_SO)
	rm -f transformed.bc transformed.ll transformed

run: $(PASS_SO)
	$(CLANG) -O2 -emit-llvm -S -o example.ll example.c
	$(OPT) -load-pass-plugin=./${PASS_SO} -passes=job-security -strings=strings.txt -labels=labels.txt -seed=1234 -o transformed.bc example.ll
	llvm-dis transformed.bc -o transformed.ll
	$(CLANG) transformed.ll -o transformed
