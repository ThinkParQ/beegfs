CXXFLAGS_COMMON := -std=c++14
CXXFLAGS_COMMON += -Wall -Wextra -Werror
CXXFLAGS_COMMON += -I include

LDFLAGS_COMMON := -std=c++14

SPHINX_BUILD := sphinx-build

tests := \
	tests/error_or.cpp

override V := $(if $V,,@)

build_type ?= asan
build-types := asan ubsan coverage
ifeq (,$(filter $(build-types), $(build_type)))
  $(error bad build type (allowed: $(build-types)))
endif

# don't require build tools for docs-only build
ifneq ($(MAKECMDGOALS),docs)
  ifneq (0,$(shell pkg-config gtest && echo 0))
    $(error need gtest)
  endif

  CXXFLAGS_COMMON += $(shell pkg-config --cflags gtest)
  LDFLAGS_COMMON += $(shell pkg-config --libs gtest)
endif

CXXFLAGS[asan] := -O1 -fsanitize=address
LDFLAGS[asan] := -fsanitize=address

CXXFLAGS[ubsan] := -O1 -fsanitize=undefined
LDFLAGS[ubsan] := -fsanitize=undefined

CXXFLAGS[coverage] := --coverage -fno-inline-functions
LDFLAGS[coverage] := --coverage

build-dir := build/$(build_type)
bin-dir := $(build-dir)/bin
obj-dir := $(build-dir)/obj

docs-dir := build/docs
docs-extracted-dir := docs/extracted
docs-source := $(wildcard include/nu/*)

override CXXFLAGS += $(CXXFLAGS_COMMON) $(CXXFLAGS[$(build_type)])
override LDFLAGS += $(LDFLAGS_COMMON) $(LDFLAGS[$(build_type)])

sources[tests] := $(tests)
objects[tests] := $(addsuffix .o, $(addprefix $(obj-dir)/, $(sources[tests])))

executables[tests] := \
	$(bin-dir)/tests/runner
objects-for[$(bin-dir)/tests/runner] := $(objects[tests])



sources := $(sources[tests])
executables := $(executables[tests])
objects := $(objects[tests])



.PHONY: all
all: $(executables)

.PHONY: clean
clean:
	rm -rf build $(docs-extracted-dir)

.PHONY: cov-check
cov-check: -cov-check-pre -cov-check-run -cov-genhtml

.PHONY: -cov-check-pre
-cov-check-pre:
	! [ -d "$(build-dir)" ] || find $(build-dir) -iname '*.gcda' -delete
	rm -f $(build-dir)/cov-info
	rm -rf $(build-dir)/cov-data

.PHONY: -cov-check-run
-cov-check-run: -cov-check-pre check

.PHONY: -cov-genhtml
-cov-genhtml: -cov-check-run
	geninfo -o $(build-dir)/cov-info --no-external --rc lcov_branch_coverage=1 -b . $(build-dir)
	genhtml -o $(build-dir)/cov-data --branch-coverage $(build-dir)/cov-info

.PHONY: check
check: $(addprefix -run-test-, $(executables[tests]))

.PHONY: check-all-profiles
check-all-profiles:
	$(MAKE) build_type=asan check
	$(MAKE) build_type=ubsan check
	$(MAKE) build_type=coverage cov-check

.PHONY: $(addprefix -run-test-, $(executables[tests]))
define declare-test-run-rule
-run-test-$1: $1
	./$1
endef
$(foreach test,$(executables[tests]),$(eval $(call declare-test-run-rule,$(test))))

.PHONY: docs
docs: $(patsubst %.hpp,$(docs-extracted-dir)/%.rst,$(docs-source)) | -create-dirs
	rm -rf $(docs-dir)
	$(SPHINX_BUILD) -n "docs" "$(docs-dir)"

$(docs-extracted-dir)/%.rst: %.hpp
	mkdir -p $(dir $@)
	awk -f docs/extract.awk <$< >$@

-print-cxxflags:
	@echo -n $(CXXFLAGS_COMMON) $(CXXFLAGS_EXTRA[$(file)])



-include $(addsuffix .d, $(objects))

-create-dirs:
	mkdir -p $(sort $(dir $(objects)))
	mkdir -p $(sort $(dir $(executables)))
	mkdir -p $(docs-dir)

$(obj-dir)/%.cpp.o: %.cpp Makefile | -create-dirs
	@echo "[CXX]	$<"
	$V$(CXX) $(CXXFLAGS) $(CXXFLAGS_EXTRA[$<]) -MM -MP -MT $@ -MF $@.d $<
	$V$(CXX) $(CXXFLAGS) $(CXXFLAGS_EXTRA[$<]) -c -o $@ $<

define declare-executable-link-rule
$1: $$(objects-for[$1]) | -create-dirs
	@echo "[LD]	$(patsubst $(bin-dir)/%,%,$1)"
	$V$$(CXX) $$(LDFLAGS) -o $$@ $$^
endef
$(foreach exe,$(executables),$(eval $(call declare-executable-link-rule,$(exe))))
