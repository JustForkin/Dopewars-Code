<?php
  include "include/dopefunc.php";
  StartHTML("Download","Download");

  function Download($url,$name='') {
    global $dnlroot;  /* Server where the files are stored - passed in */
    if ($name=='') $name = $url;
    print "   <a href=\"$dnlroot$url\">$name</a>\n";
  }
?>

<p>dopewars should compile on practically any UNIX system which has the
(n)curses library. It can also be compiled on Windows systems; see below.
If you're running Fedora Core 4 on an Intel-based system, you can install
dopewars by getting the relevant RPM(s) and reading the README file. If you
don't have RPM or they don't install properly, grab the tarball instead.</p>

<p>Latest stable version is <b>1.5.12</b>. The RPMs may be a little out of date,
depending on how often I bother to rebuild them... So check the version
numbers!</p>

<p>All RPMs are <a href="http://www.gnupg.org/">GnuPG</a> signed. My public
key is available
<a href="http://dopewars.sourceforge.net/pubkey.txt">here</a>.
MD5 checksums of all downloadable material are also available
<a href="http://dopewars.sourceforge.net/md5-1.5.12.txt">here</a>.
</p>

<h3>Microsoft Windows</h3>
<p>Windows installer (Win95, Win98, WinNT, Win2000, WinME, WinXP),
version 1.5.12: &nbsp;
<?php Download("dopewars-1.5.12.exe"); ?>
<br />
(just run this program, and it will install dopewars automatically for you;
if you're worried about Internet Explorer's "security" warning, see the
<a href="faq.html#windows">FAQ</a>, or check its
<?php Download("dopewars-1.5.12.exe.gpg", "GPG signature"); ?>
</p>

<h3>RedHat Linux (and similar)</h3>
<p>Binary RPM for Fedora Core 4/Intel, version 1.5.12: &nbsp;
<?php Download("dopewars-1.5.12-1.i386.rpm"); ?>
<br />
SDL_mixer (Simple DirectMedia Layer) sound plugin: &nbsp;
<?php Download("dopewars-sdl-1.5.12-1.i386.rpm"); ?>
</p>

<h3>Source code</h3>
<p>Source RPM, version 1.5.12: &nbsp;
<?php Download("dopewars-1.5.12-1.src.rpm"); ?>
</p>

<p>Source code tarball, version 1.5.12: &nbsp;
<?php Download("dopewars-1.5.12.tar.gz"); ?>
<?php Download("dopewars-1.5.12.tar.gz.gpg", "(GPG)"); ?>
</p>

<h3>Other Linux/Unix</h3>

<p><i>These packages are built by third parties, and so may not be as up to date
as the latest release.</i></p>

<p>
<?php Download("dopewars-1.5.2-slackware.tar.gz","Slackware package"); ?>
</p>

<!-- <p><a href="http://lis.snv.jussieu.fr/~rousse/linux/plf/">Mandrake
package</a></p> -->

<p><a href="http://packages.debian.org/unstable/games/dopewars.html">Debian
package</a></p>

<p><a href="http://pdb.finkproject.org/pdb/package.php/dopewars">Mac OS X
(Fink) package</a></p>

<!--
<p><a href="http://www.lindows.com/products_details/product_1310.html">Lindows
Click-N-Run package</a></p> -->

<p><a href="http://www.freebsd.org/cgi/url.cgi?ports/games/dopewars/pkg-descr">
FreeBSD port</a></p>

<p><a href="ftp://ftp.netbsd.org/pub/NetBSD/packages/pkgsrc/games/dopewars/README.html">NetBSD package</a></p>

<p><a href="http://www.openbsd.org/3.6_packages/i386/dopewars-1.5.7p2.tgz-long.html">OpenBSD i386 package</a></p>

<p>Foresight Linux or Specifix Linux: run <tt>conary update dopewars</tt></p>

<h3>Documentation etc.</h3>

<p>Text documentation: &nbsp;
   <a href="README">README</a>
</p>

<p>List of changes in this and earlier versions (ChangeLog): &nbsp;
   <a href="ChangeLog">ChangeLog</a>
</p>

<p>Example configuration file for dopewars: &nbsp;
   <a href="example-cfg">example-cfg</a>
</p>

<h2><a id="develop">Development versions</a></h2>
<p>dopewars is in continual development, and so, to take advantage of
new features, or to contribute to the code and/or translations, you can get
the latest version of the code by
<a href="http://sourceforge.net/docs/E09">Subversion</a> (SVN) from
<a href="http://sourceforge.net/">SourceForge</a>. Be warned that this
code may not be fully functional - that's why it's still in development!</p>

<p>See <a href="http://svnbook.red-bean.com/">the O'Reilly book</a> for more
details about using Subversion.</p>

<p>To obtain the whole code from scratch, use the following command, which will
extract the latest SVN version into the directory <tt>dopewars</tt> below your
current directory:</p>

<pre class="unixcom">svn co https://dopewars.svn.sourceforge.net/svnroot/dopewars/dopewars/trunk dopewars</pre>

<p>If you already have a copy of the dopewars SVN and wish to bring it up to
date, change into the <tt>dopewars</tt> directory and then run the following
command:</p>

<pre class="unixcom">svn up</pre>

<p><a href="mailto:benwebb@users.sf.net">Feedback</a> is, as always,
appreciated, so that I can iron out any bugs in the code or configure script
before the next "stable" release.</p>

<h2>Old versions</h2>
<p>If you desperately want them, all previous GPL versions of dopewars are also
available, mainly at the <a
href="http://sourceforge.net/project/showfiles.php?group_id=11128">SourceForge
files page</a>. Very old versions are also available below. N.B. It is not
recommended to use these versions for anything other than testing - they all
contain bugs, including some severe security problems!</p>

<p>
<a href="oldversions/dopewars-1.4.7.tar.gz">Version 1.4.7.</a>
 Released: 14-01-00<br />
<a href="oldversions/dopewars-1.4.6.tar.gz">Version 1.4.6.</a>
 Released: 12-11-99<br />
<a href="oldversions/dopewars-1.4.5.tar.gz">Version 1.4.5.</a>
 Released: 21-10-99<br />
<a href="oldversions/dopewars-1.4.4.tar.gz">Version 1.4.4.</a>
 Released: 16-9-99<br />
<a href="oldversions/dopewars-1.4.3.tar.gz">Version 1.4.3.</a>
 Released: 23-6-99<br />
<a href="oldversions/dopewars-1.4.2.tar.gz">Version 1.4.2.</a>
 Released: 16-5-99<br />
<a href="oldversions/dopewars-1.4.1b.tar.gz">Version 1.4.1b.</a>
 Released: 28-4-99<br />
<a href="oldversions/dopewars-1.4.1a.tar.gz">Version 1.4.1a.</a>
 Released: 28-4-99<br />
<a href="oldversions/dopewars-1.4.1.tar.gz">Version 1.4.1.</a>
 Released: 27-4-99<br />
<a href="oldversions/dopewars-1.4.0.tar.gz">Version 1.4.0</a>
 Released: 27-4-99<br />
</p>

<?php
  WriteNavLinks("Download");
  EndHTML();
?>
