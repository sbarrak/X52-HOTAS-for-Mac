# X52 Pro for X-Plane 12 on Apple Silicon (arm64).
#   make              build everything into bin/ and run the tests
#   make test         run the unit tests
#   make install-app  put the app in /Applications (it installs the plugin into X-Plane itself)
#   make install      copy the plugin straight into X-Plane
#   make release      bin/X52-Pro-<version>.zip, ready to share (the version is in the VERSION file)
# Built programs land in bin/, intermediate files and test programs in build/.

# X-Plane keeps a list of its installs; use the first one, or set XPLANE="/path/to/X-Plane 12".
XPLANE ?= $(shell sed -n '1{p;q;}' "$$HOME/Library/Preferences/x-plane_install_12.txt" 2>/dev/null)
SDK     = third_party/XPSDK
ARCH    = -arch arm64 -mmacosx-version-min=12.0
CXX     = clang++
VERSION  = $(shell cat VERSION)
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra $(ARCH) -fvisibility=hidden -DX52_VERSION='"$(VERSION)"'

CORE   = src/core/profile.cpp src/core/mfd.cpp src/core/pages.cpp src/core/sidepages.cpp src/core/keys.cpp src/core/engine.cpp src/core/catalog.cpp src/core/xpjoy.cpp src/core/pagetree.cpp src/core/pageview.cpp src/core/axes.cpp src/core/update.cpp
MAC    = src/mac/x52device.cpp src/mac/x52input.cpp src/mac/keysend.cpp src/mac/xplane.cpp src/mac/simlink.cpp
PLUGIN = src/plugin/plugin.cpp src/plugin/ui.cpp src/plugin/store.cpp
AGENT  = src/app/agent.cpp src/plugin/store.cpp
HEADERS = $(wildcard src/core/*.h src/mac/*.h src/plugin/*.h src/app/*.h)

XPDEFS = -DXPLM200=1 -DXPLM210=1 -DXPLM300=1 -DXPLM301=1 -DXPLM303=1 -DXPLM400=1 -DAPL=1 -DIBM=0 -DLIN=0 \
         -DGL_SILENCE_DEPRECATION
XPINC  = -I$(SDK)/CHeaders/XPLM -I$(SDK)/CHeaders/Widgets
FRAMEWORKS = -framework IOKit -framework CoreFoundation -framework ApplicationServices
BIN = bin
XPL = $(BIN)/X52Pro.xpl

APP = $(BIN)/X52Pro.app
APPBIN = $(APP)/Contents/MacOS/X52Pro
APPSRC = src/app/bridge.cpp $(AGENT) $(CORE) $(MAC)

all: test $(XPL) $(BIN)/x52probe app

build/test_core: tests/test_core.cpp tests/check.h $(CORE) $(HEADERS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ tests/test_core.cpp $(CORE)

build/test_plugin: tests/test_plugin.cpp tests/fake_xplm.cpp tests/fake_xplm.h tests/check.h $(PLUGIN) $(CORE) $(MAC) $(HEADERS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(XPDEFS) $(XPINC) -DX52_NO_GL -DX52_NO_HID -o $@ tests/test_plugin.cpp tests/fake_xplm.cpp \
		$(PLUGIN) $(CORE) $(MAC) $(FRAMEWORKS)

build/test_agent: tests/test_agent.cpp tests/check.h $(AGENT) src/app/bridge.cpp $(CORE) $(MAC) $(HEADERS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ tests/test_agent.cpp $(AGENT) src/app/bridge.cpp $(CORE) $(MAC) $(FRAMEWORKS)

build/test_xplane: tests/test_xplane.cpp tests/check.h src/mac/xplane.cpp src/mac/xplane.h
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ tests/test_xplane.cpp src/mac/xplane.cpp

build/test_simlink: tests/test_simlink.cpp tests/check.h src/mac/simlink.cpp src/mac/simlink.h
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ tests/test_simlink.cpp src/mac/simlink.cpp

test: build/test_core build/test_plugin build/test_agent build/test_xplane build/test_simlink
	./build/test_simlink
	./build/test_xplane
	./build/test_core
	./build/test_plugin
	./build/test_agent

$(BIN)/x52probe: tools/x52probe.cpp $(CORE) $(MAC) $(HEADERS)
	@mkdir -p $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ tools/x52probe.cpp $(CORE) $(MAC) $(FRAMEWORKS)

$(XPL): $(PLUGIN) $(CORE) $(MAC) $(HEADERS) VERSION
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(XPDEFS) $(XPINC) -bundle -o $@ $(PLUGIN) $(CORE) $(MAC) \
		-F$(SDK)/Libraries/Mac -framework XPLM $(FRAMEWORKS) -framework OpenGL
	codesign --force --sign - $@

# The menu bar app: C++ compiled to one object, linked with the Swift window code.
app: $(APPBIN)
build/AppIcon.icns: tools/make_icon.swift
	@mkdir -p build
	rm -rf build/AppIcon.iconset
	swift tools/make_icon.swift build/AppIcon.iconset
	iconutil -c icns build/AppIcon.iconset -o $@

$(APPBIN): src/app/main.swift src/app/Info.plist $(APPSRC) $(HEADERS) $(XPL) build/AppIcon.icns VERSION
	@mkdir -p "$(APP)/Contents/MacOS" build/appobj
	cd build/appobj && $(CXX) $(CXXFLAGS) -mmacosx-version-min=13.0 -c $(addprefix ../../,$(APPSRC))
	swiftc -O -target arm64-apple-macos13.0 -import-objc-header src/app/bridge.h src/app/main.swift \
		build/appobj/*.o -lc++ $(FRAMEWORKS) -o "$(APPBIN)"
	mkdir -p "$(APP)/Contents/Resources"
	cp $(XPL) "$(APP)/Contents/Resources/X52Pro.xpl"
	cp build/AppIcon.icns "$(APP)/Contents/Resources/AppIcon.icns"
	cp src/app/Info.plist "$(APP)/Contents/Info.plist"
	/usr/libexec/PlistBuddy -c "Set :CFBundleShortVersionString $(VERSION)" -c "Set :CFBundleVersion $(VERSION)" "$(APP)/Contents/Info.plist"
	codesign --force --sign - "$(APP)"

install-app: app
	-pkill -f "X52 Pro.app/Contents/MacOS/X52Pro"
	rm -rf "/Applications/X52 Pro.app"
	cp -R "$(APP)" "/Applications/X52 Pro.app"
	@echo "Installed /Applications/X52 Pro.app"

# Copy next to the old file and swap it in, so a running X-Plane is not disturbed.
install: $(XPL)
	@test -n "$(XPLANE)" -a -d "$(XPLANE)/Resources/plugins" || { echo 'X-Plane 12 not found: run make install XPLANE="/path/to/X-Plane 12"'; exit 1; }
	mkdir -p "$(XPLANE)/Resources/plugins/X52Pro/mac_x64"
	cp $(XPL) "$(XPLANE)/Resources/plugins/X52Pro/mac_x64/X52Pro.xpl.new"
	mv -f "$(XPLANE)/Resources/plugins/X52Pro/mac_x64/X52Pro.xpl.new" "$(XPLANE)/Resources/plugins/X52Pro/mac_x64/X52Pro.xpl"
	@echo "Installed to $(XPLANE)/Resources/plugins/X52Pro"

release: app
	rm -f $(BIN)/X52-Pro*.zip
	cd $(BIN) && rm -rf "X52 Pro.app" && cp -R X52Pro.app "X52 Pro.app" && ditto -c -k --keepParent "X52 Pro.app" X52-Pro-$(VERSION).zip && rm -rf "X52 Pro.app"
	@echo "Made $(BIN)/X52-Pro-$(VERSION).zip"

clean:
	rm -rf build $(BIN)

.PHONY: all test install clean app install-app release
