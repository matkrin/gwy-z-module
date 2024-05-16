# gwy-z-module

On Arch, first install gwyddion from the AUR.
This should install all relevant packages.

Create build files with:
```bash
./autogen.sh
```

Then you can run
```bash
./configure --with-dest=home && make && make install
```
to install in `$HOME/.gwyddion/modules/process`



On Ubuntu 22.04 I had success with installing the following dependecies:
```bash
sudo apt install gwyddion gwyddion-devel libgwyddion-dev libgwyddion20-dev gtk+2.0 libgtk2.0-dev fftw3-dev libgtkglext1-dev
```

## Docker

Build the image from the Dockerfile:
```bash
sudo docker build -t <tag-name> .
```

Run container interactively:
```bash
sudo docker run -it <tag-name>
```

In container:
```bash
make clean && make distclean
./configure CFLAGS='-fstack-protector' --host=i686-w64-mingw32 --build=$(/usr/share/libtool/config/config.guess) PKG_CONFIG_PATH=/usr/i686-w64-mingw32/sys-root/mingw/lib --with-dest="/gwy-z-module/dist"
```

### LSP support

For clangd support create compile_commands.json with [bear]:

```bash
make clean && make distclean
./configure --with-dest=home
bear -- make
```

[bear]: https://github.com/rizsotto/Bear


---

This is an example of standalone Gwyddion module.

It doesn't do anything extremely useful, but is complex enough to demostrate
the basic principles.

Direct questions to gwyddion-devel@lists.sourceforge.net or to
yeti@gwyddion.net.

Since version 2.6 the module uses parameter handling functions available in
Gwyddion 2.59+.  If you wish to write a module compatible with old Gwyddion
versions see version 2.5 instead.


== Unix ======

If you have Gwyddion installed in a non-standard location set PKG_CONFIG_PATH
to the directory where gwyddion.pc resides:

    export PKG_CONFIG_PATH=/home/me/opt/gwyddion/lib/pkgconfig

Run configure

    ./configure [OPTIONS...]

The build system can handle several common types of installation, controlled
by --with-dest=WHERE option which can take the following values:

    WHERE=system (the default if you don't specify --prefix)
        the module to be installed into Gwyddion system module directory
        according to its type (process, file, ...), i.e. the same directory
        where the modules that came with Gwyddion reside

    WHERE=prefix (the default if you specify --prefix)
        the module to be installed into Gwyddion system module directory but
        under given prefix, i.e. with --prefix=/usr/local the module might be
        installed into /usr/local/lib/gwyddion/modules/process regardless
        where Gwyddion itself resides

    WHERE=home
        the module will be installed into $HOME/.gwyddion/modules according
        to its type

    WHERE=/foo/bar
        the module will be installed to exactly this directory (it must be
        and absolute path)

Running

    make
    make install

compiles and installs the module; running

    make uninstall

uninstalls it.


== MinGW32 Cross-Compilation for MS Windows ======

This has only been tested with Fedora cross-compilation support. You need to
install the mingw32-gwyddion-libs package that contains the cross-compiled
versions of Gwyddion libraries.

Specify the host system to configure for 32bit

    ./configure --host=i686-w64-mingw32 --build=$(/usr/share/libtool/config/config.guess) PKG_CONFIG_PATH=/usr/i686-w64-mingw32/sys-root/mingw/lib [OPTIONS...]

or 64bit

    ./configure --host=x86_64-w64-mingw32 --build=$(/usr/share/libtool/config/config.guess) PKG_CONFIG_PATH=/usr/x86_64-w64-mingw32/sys-root/mingw/lib [OPTIONS...]

and then continue as in a normal Unix compilation to produce a MS Windows
DLL of the module.

In order to create a usable DLL you will likely need to run `make install'
so use --with-dest option of configure to specify (an arbitrary) destination
directory, see above.


== SVN Checkout & Development ======

Starting from a fresh subversion checkout is the recommended course of action
if you are going to use threshold-example as a base for your own standalone
module as it ensures you have all the build tools.

Check out the latest version from subversion:

    https://gwyddion.svn.sourceforge.net/svnroot/gwyddion/trunk/threshold-example

Initialise the build system:

    ./autogen.sh [OPTIONS...]

You will need libtool, automake, autoconf, ..., the usual lot.  Give
autogen.sh any options you would give to configure (see above).

See the top of configure.ac and Makefile.am for notes about things you will
probably need to modify if you start developing your own module based on
threshold-example.


