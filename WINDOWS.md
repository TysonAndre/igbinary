Compiling igbinary from source on Windows
=========================================

This guide is added for packagers or developers of the igbinary project.

Installing through pecl is probably easier for users of igbinary?

Setting up a Windows VM
-----------------------

If you don't have windows installed:

1. Download a VM from https://developer.microsoft.com/en-us/microsoft-edge/tools/vms/ (E.g. Windows 7 IE11)
   Prefer Windows 7. The trial period can be extended.
2. Install the VM. Keep the disk image around, the VM trial period expires in 30 days.
3. Run Windows Updates, which will take a long time.
4. Read the instructions on the desktop background.
5. (Windows 7)Search for "Command Prompt", right click on it and click "Run as administrator", and execute `slmgr /ato` to extend the trial period to 90 days.

If the license expires/is about to expire , it is possible to "re-arm" (extend) the license several times with `slmgr`
Installing Visual Studio
------------------------

1. Download the version of Visual Studio needed to build the newest version of PHP (mentioned on https://wiki.php.net/internals/windows/stepbystepbuild#building_pecl_extensions)
   At the time of writing, that was [Visual Studio Community Edition 2015](https://www.visualstudio.com/products/visual-studio-community-vs)

   VirtualBox
2. Install Visual Studio 2015. (Use custom installation, make sure that all C++ features are enabled)

Downloading PHP
---------------

See https://wiki.php.net/internals/windows/stepbystepbuild

Prefer downloading stable releases of php?

(This hasn't been tested yet on the suggested VM)


Building igbinary
-----------------

See https://wiki.php.net/internals/windows/stepbystepbuild#building\_pecl\_extensions

### Install APCU (optional?)

Note: The Windows build hasn't worked for a while in 5.6, so there may be compilation errors or test failures.

TODO: Make sure that `config.w32` works without APCU installed.

> 1. Open the extension's page on PECL ( APCu)
> 2. Download the extension source either by:
> 
>    -    downloading a source archive
>    -    fetching the source from the extension's repository (link can be found under *Browse Source*)
> 3. Create a directory named pecl on the same level as your PHP source directory, e.g. C:\php-sdk\phpdev\vc11\x86\pecl
> 4. Extract or clone the extension source code to the pecl directory
> 
>    - if cloning, clone to a subdirectory, e.g. C:\php-sdk\phpdev\vc11\x86\pecl\apcu
>    - source code archive should already contain a subdirectory named e.g. apcu-4.0.7
> 5. Open a command prompt, run the setvars script, and enter your PHP source directory
> 6. Rebuild the configure script by running:
> 
>    ```
>    buildconf
>    ```
> 
> 7. Executing `configure --help` should now contain an option to enable APCu
> 
>    ```
>    --enable-apcu    Whether to enable APCu support
>    ```
> 
> 8. Configure and build:
> 
>    ```
>    configure --disable-all --enable-cli --enable-apcu
>    ```
> 
>    ```
>    nmake
>    ```
> 
> 9. Test the binary with a php -m command, to make sure APCu is loaded

### Install igbinary

Similar to the above instructions.

1. (This isn't released yet, skip this step and download from github)
   Open the extension's page on PECL ( APCu)
2. Download the extension source either by:

   -    downloading a source archive
   -    fetching the source from the extension's repository (link can be found under *Browse Source*)
        https://github.com/igbinary/igbinary/
3. Create a directory named pecl on the same level as your PHP source directory, e.g. C:\php-sdk\phpdev\vc11\x86\pecl
4. Extract or clone the extension source code to the pecl directory

   - if cloning, clone to a subdirectory, e.g. C:\php-sdk\phpdev\vc11\x86\pecl\apcu
   - source code archive should already contain a subdirectory named e.g. apcu-4.0.7
5. Open a command prompt, run the setvars script, and enter your PHP source directory
6. Rebuild the configure script by running:

   ```
   buildconf
   ```

7. Executing `configure --help` should now contain options.
   (`config.m4` currently has no options. May allow disabling apcu support in the future, etc.)

   ```

   ```

8. Configure and build:
   One may want to set CFLAGS to -O2 so it will be faster?

   ```
   configure
   ```

   ```
   nmake
   ```

9. Test the binary with a php -m command, to make sure APCu is loaded
10. Run the unit tests. Most should pass, some should be skipped (Or be expected failures(XFAIL)), none should fail.

    ```
    nmake test
	```
