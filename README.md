# ![Icon](http://injectionforxcode.johnholdsworth.com/mtar.png) mtar - Multi-threaded tar

Copyright (c) Original authors & John Holdsworth 2013

Initial version of "mtar", a modified verion of the bsdtar sources to multi-thread archive creation.
mtar creates archives fully compatable with tar, sometimes dramatically faster.

To enable mutli-threading add the option "Y" to your tar command e.g.:

	./bsdtar cfvzY tarfile.tgz dir1 dir2 etc.

To improve archiving speed two optimisations have been made to the source code. Reading
of input files has been multi-threaded for improved performance on slow file systems or
when used on network file shares. More importantly compression of the archive created
has been multi-threaded by incorporating the "pigz" project for dramatic reductions 
in archive creation times on modern multi-cored architectures. 

"mtar" is a modified version of the following software:

    bsdtar version 3.1.2 from http://www.libarchive.org/

    pigz version 2.3 from http://zlib.net/pigz/

## Building mtar

To build, clone the repository using the command:

    git clone git://github.com/johnno1962/mtar.git

or download the tar ball and extract it then cd into the "mtar" directory and type:

    ./configure
    
then, the following will create the binary "./bsdtar":

    make

On Linux you will need to manually edit Makefile to add "-lpthread" to the end of
the line containing "LDFLAGS =" and rebuild. You may also need to install zlib
using the command:

    sudo apt-get install --reinstall zlibc zlib1g zlib1g-dev

## Bug reports

This is the initial version of mtar. Please file bug reports to [mtar@johnholdsworth.com](mailto:mtar@johnholdsworth.com)
or fork and enter pull requests with any improvements on github in the usual way.
I should warn you the way pigz has been incorporated via a #include from libarchive/archive_write_add_filter_gzip.c
is a little untidy to keep changes to the project Makefile to a minimum (I also couldn't
get autoconf to build on my mac!) 

## Please note:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

