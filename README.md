For build prereqs, see [the CS144 VM setup instructions](https://web.stanford.edu/class/cs144/vm_howto).

## Sponge quickstart

To set up your build directory:

	$ mkdir -p <path/to/sponge>/build
	$ cd <path/to/sponge>/build
	$ cmake ..

**Note:** all further commands listed below should be run from the `build` dir.

To build:

    $ make

You can use the `-j` switch to build in parallel, e.g.,

    $ make -j$(nproc)

To test (after building; make sure you've got the [build prereqs](https://web.stanford.edu/class/cs144/vm_howto) installed!)

    $ make check

The first time you run `make check`, it will run `sudo` to configure two
[TUN](https://www.kernel.org/doc/Documentation/networking/tuntap.txt) devices for use during
testing.

### build options

You can specify a different compiler when you run cmake:

    $ CC=clang CXX=clang++ cmake ..

You can also specify `CLANG_TIDY=` or `CLANG_FORMAT=` (see "other useful targets", below).

Sponge's build system supports several different build targets. By default, cmake chooses the `Release`
target, which enables the usual optimizations. The `Debug` target enables debugging and reduces the
level of optimization. To choose the `Debug` target:

    $ cmake .. -DCMAKE_BUILD_TYPE=Debug

The following targets are supported:

- `Release` - optimizations
- `Debug` - debug symbols and `-Og`
- `RelASan` - release build with [ASan](https://en.wikipedia.org/wiki/AddressSanitizer) and
  [UBSan](https://developers.redhat.com/blog/2014/10/16/gcc-undefined-behavior-sanitizer-ubsan/)
- `RelTSan` - release build with
  [ThreadSan](https://developer.mozilla.org/en-US/docs/Mozilla/Projects/Thread_Sanitizer)
- `DebugASan` - debug build with ASan and UBSan
- `DebugTSan` - debug build with ThreadSan

Of course, you can combine all of the above, e.g.,

    $ CLANG_TIDY=clang-tidy-6.0 CXX=clang++-6.0 .. -DCMAKE_BUILD_TYPE=Debug

**Note:** if you want to change `CC`, `CXX`, `CLANG_TIDY`, or `CLANG_FORMAT`, you need to remove
`build/CMakeCache.txt` and re-run cmake. (This isn't necessary for `CMAKE_BUILD_TYPE`.)

### other useful targets

To generate documentation (you'll need `doxygen`; output will be in `build/doc/`):

    $ make doc

To lint (you'll need `clang-tidy`):

    $ make -j$(nproc) tidy

To run cppcheck (you'll need `cppcheck`):

    $ make cppcheck

To format (you'll need `clang-format`):

    $ make format

To see all available targets,

    $ make help
6 - t_recv_window (Failed)
34 - t_ack_rst (Failed)
44 - t_retx_win (Failed)
47 - t_reorder (Failed)



	  6 - t_recv_window (Failed)
	 28 - t_webget (Failed)
	 34 - t_ack_rst (Failed)
	 44 - t_retx_win (Failed)
	 47 - t_reorder (Failed)
	 57 - t_ucS_1M_32k (Failed)
	 58 - t_ucS_128K_8K (Failed)
	 61 - t_ucR_1M_32k (Failed)
	 62 - t_ucR_128K_8K (Failed)
	 65 - t_ucD_1M_32k (Failed)
	 66 - t_ucD_128K_8K (Failed)
	 68 - t_ucD_32K_d (Timeout)
	 69 - t_usS_1M_32k (Failed)
	 70 - t_usS_128K_8K (Failed)
	 73 - t_usR_1M_32k (Failed)
	 74 - t_usR_128K_8K (Failed)
	 77 - t_usD_1M_32k (Failed)
	 78 - t_usD_128K_8K (Failed)
	 80 - t_usD_32K_d (Timeout)
	 81 - t_ucS_128K_8K_l (Failed)
	 82 - t_ucS_128K_8K_L (Timeout)
	 83 - t_ucS_128K_8K_lL (Timeout)
	 84 - t_ucR_128K_8K_l (Timeout)
	 85 - t_ucR_128K_8K_L (Failed)
	 86 - t_ucR_128K_8K_lL (Timeout)
	 87 - t_ucD_128K_8K_l (Failed)
	 88 - t_ucD_128K_8K_L (Failed)
	 89 - t_ucD_128K_8K_lL (Failed)
	 90 - t_usS_128K_8K_l (Failed)
	 91 - t_usS_128K_8K_L (Timeout)
	 92 - t_usS_128K_8K_lL (Timeout)
	 93 - t_usR_128K_8K_l (Failed)
	 94 - t_usR_128K_8K_L (Failed)
	 95 - t_usR_128K_8K_lL (Timeout)
	 96 - t_usD_128K_8K_l (Failed)
	 97 - t_usD_128K_8K_L (Failed)
	 98 - t_usD_128K_8K_lL (Timeout)
	 99 - t_ipv4_client_send (Failed)
	100 - t_ipv4_server_send (Timeout)
	101 - t_ipv4_client_recv (Timeout)
	102 - t_ipv4_server_recv (Failed)
	103 - t_ipv4_client_dupl (Failed)
	104 - t_ipv4_server_dupl (Failed)
	105 - t_icS_1M_32k (Failed)
	106 - t_icS_128K_8K (Failed)
	107 - t_icS_16_1 (Failed)
	108 - t_icS_32K_d (Failed)
	109 - t_icR_1M_32k (Timeout)
	110 - t_icR_128K_8K (Timeout)
	111 - t_icR_16_1 (Timeout)
	112 - t_icR_32K_d (Timeout)
	113 - t_icD_1M_32k (Failed)
	114 - t_icD_128K_8K (Failed)
	115 - t_icD_16_1 (Failed)
	116 - t_icD_32K_d (Failed)
	117 - t_isS_1M_32k (Timeout)
	118 - t_isS_128K_8K (Timeout)
	119 - t_isS_16_1 (Timeout)
	120 - t_isS_32K_d (Timeout)
	121 - t_isR_1M_32k (Failed)
	122 - t_isR_128K_8K (Failed)
	123 - t_isR_16_1 (Failed)
	124 - t_isR_32K_d (Failed)
	125 - t_isD_1M_32k (Failed)
	126 - t_isD_128K_8K (Failed)
	127 - t_isD_16_1 (Failed)
	128 - t_isD_32K_d (Failed)
	129 - t_icS_128K_8K_l (Failed)
	130 - t_icS_128K_8K_L (Failed)
	131 - t_icS_128K_8K_lL (Failed)
	132 - t_icR_128K_8K_l (Timeout)
	133 - t_icR_128K_8K_L (Timeout)
	134 - t_icR_128K_8K_lL (Timeout)
	135 - t_icD_128K_8K_l (Failed)
	136 - t_icD_128K_8K_L (Failed)
	137 - t_icD_128K_8K_lL (Failed)
	138 - t_isS_128K_8K_l (Timeout)
	139 - t_isS_128K_8K_L (Timeout)
	140 - t_isS_128K_8K_lL (Timeout)
	141 - t_isR_128K_8K_l (Failed)
	142 - t_isR_128K_8K_L (Failed)
	143 - t_isR_128K_8K_lL (Failed)
	144 - t_isD_128K_8K_l (Failed)
	145 - t_isD_128K_8K_L (Failed)
	146 - t_isD_128K_8K_lL (Failed)
	148 - t_icnS_128K_8K_L (Timeout)
	149 - t_icnS_128K_8K_lL (Timeout)
	150 - t_icnR_128K_8K_l (Timeout)
	151 - t_icnR_128K_8K_L (Timeout)
	152 - t_icnR_128K_8K_lL (Timeout)
	153 - t_icnD_128K_8K_l (Timeout)
	154 - t_icnD_128K_8K_L (Timeout)
	155 - t_icnD_128K_8K_lL (Timeout)
	157 - t_isnS_128K_8K_L (Timeout)
	158 - t_isnS_128K_8K_lL (Timeout)
	159 - t_isnR_128K_8K_l (Timeout)
	160 - t_isnR_128K_8K_L (Timeout)
	161 - t_isnR_128K_8K_lL (Timeout)
	162 - t_isnD_128K_8K_l (Timeout)
	163 - t_isnD_128K_8K_L (Timeout)
	164 - t_isnD_128K_8K_lL (Timeout)
