all:
	$(MAKE) -C ./src all
	cp ./src/rtexp_module.so .

test:
	$(MAKE) -C ./src $@

clean:
	$(MAKE) -C ./src $@

distclean:
	$(MAKE) -C ./src $@
.PHONY: distclean

package: all
	$(MAKE) -C ./src package
.PHONY: package

buildall:
	$(MAKE) -C ./src $@

staticlib:
	$(MAKE) -C ./src $@

# Builds a small utility that outputs the current version
print_version:
	$(MAKE) -C ./src print_version

rebuild: clean all
