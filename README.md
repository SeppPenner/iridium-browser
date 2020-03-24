Iridium Browser
==============================

Iridium is an open modification of the Chromium code base, with privacy being enhanced in several key areas. Automatic transmission of partial queries, keywords, metrics to central services is inhibited and only occurs with consent.

Some more information and binary downloads can be found at [https://iridiumbrowser.de](https://iridiumbrowser.de).

## Development

The repository is at [https://github.com/iridium-browser/iridium-browser/](https://github.com/iridium-browser/iridium-browser/).

The easy way to build from source is to re-use the mechanisms of one's Linux distribution, that is, their build environment and description for chromium, and replacing the source tarball by the one from [https://dl.iridiumbrowser.de/source/](https://dl.iridiumbrowser.de/source/).

For the more adventurous, there is a 10-step(-ish) guide to build from git:

1. Download `[https://chromium.googlesource.com/chromium/tools/depot_tools.git](depot_tools)` and add it to your `$PATH`.

2. Issue command: `gclient sync` (this utility comes from depot_tools). For gclient, you need to have python 2(!) and python2-virtualenv. The sync command will not necessary complete successfully due to webrtc.

3. Download matching WebRTC code. Google populates the Chromium `DEPS` file with a commit hash for WebRTC that not always a branch head which, as a result, is not downloaded by `gclient sync`. Therefore, you have to fetch the data manually:

	pushd third_party/webrtc/
	git ls-remote origin | grep 0b2302e5e0418b6716fbc0b3927874fd3a842caf
	    abcdef...   refs/branch-heads/m78
	git fetch origin refs/branch-heads/m78
	git reset --hard FETCH_HEAD
	popd

4. The gn files in Iridium are edited to respect the `$CC` etc. environment variables. These env vars _must_ always be set, so issue `export CC=gcc CXX=g++ AR=ar NM=nm` (can pick any preferred toolchain, though).

5. Link up nodejs:

	mkdir -p third_party/node/linux/node-linux-x64/bin
	ln -s /usr/bin/node third_party/node/linux/node-linux-x64/bin/

6. Issue `gn gen '--args= custom_toolchain="//build/toolchain/linux/unbundle:default" host_toolchain="//build/toolchain/linux/unbundle:default" linux_use_bundled_binutils=false use_custom_libcxx=false is_debug=false enable_nacl=false use_swiftshader_with_subzero=true is_component_ffmpeg=true use_cups=true use_aura=true concurrent_links=1 symbol_level=1 blink_symbol_level=0 use_kerberos=true enable_vr=false optimize_webui=false enable_reading_list=false use_pulseaudio=true link_pulseaudio=true is_component_build=false use_sysroot=false fatal_linker_warnings=false use_allocator="tcmalloc" fieldtrial_testing_like_official_build=true use_gold=true use_gnome_keyring=false use_lld=false use_vaapi=true use_sysroot=false treat_warnings_as_errors=false enable_widevine=false use_dbus=true use_system_harfbuzz=true use_system_freetype=true enable_hangout_services_extension=true enable_vulkan=true enable_hevc_demuxing=true rtc_use_pipewire=true rtc_link_pipewire=true is_clang=false gcc_lto=true icu_use_data_file=false proprietary_codecs=true ffmpeg_branding="Chrome"' out/Release`

7. There are a number of dependencies, and they may vary across operating systems. The gn command fails if there are unmet dependencies, and it will tell you which. Install and repeat the gn command as needed. Consult your distribution's package manager.

8. Issue `LD_LIBRARY_PATH=$PWD/out/Release ninja -C out/Release chrome chrome_sandbox chromedriver`.

## Reporting bugs and issues

Please use the [Iridium Browser tracker](https://github.com/iridium-browser/iridium-browser/issues) on GitHub to report your findings or to add feature requests.
