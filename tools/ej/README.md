EJ (short for “Easy Journal”) is the document system that MaraDNS
uses; this is in response to my translators asking for a single unified
document format which can be converted in to the following three formats:

* HTML documents (albeit with minimal styling)
* Man pages
* Plain text documents

The EJ tools were originally written in Perl in 2002.  In 2022, the
tools were re-written in Lua 5.1 so that building MaraDNS’s documents
no longer need a non-POSIX tool which isn’t included with MaraDNS.

# Getting the EJ tools to work

The EJ tools are Lua scripts, written for Lua 5.1.  Should the scripts
not run, make sure to have either Lua 5.1 (a standard package for 
man *NIX distributions) or Lunacy (see below) installed.

MaraDNS includes a full fork of Lua5.1 called `lunacy`, to compile it,
enter the `coLunacyDNS/lunacy` directory, use `make` to compile `lunacy`,
then, as root `cp lunacy /usr/local/bin`.

# An example EJ document

To get an overview of how EJ works, here’s an example EJ document:

```
<HEAD>
<TH>EXAMPLE 1 "October 2022" EXAMPLE "Example reference"</TH>
<META HTTP-EQUIV="Content-Type" CONTENT="text/html; CHARSET=utf-8">
<!-- This is a multi-line comment.
     This is a second line in the comment. -->
</HEAD>
<BODY>

<H1>NAME</H1>
Example - An example EJ document

<H1>DESCRIPTION</H1>

This is an example document showing the general format of EJ documents.

<P>

This is a second paragraph.

<p>

Here are some angled brackets: &lt; &gt;

<PRE>
-- Here is a preformatted block of text
-- Tags like <THIS> are passed as-is.  As is &lt;
for a=1,10 do
    print(a)
end
</PRE>

<UL>
<LI>This is a bullet item

<LI>As is this
</UL>

<B>This is some bold text</B>
<p>
<I>This is some italic text</I>
<p>

<h1>LICENSE</h1>
This example document is public domain.

<h1>AUTHOR</h1>
Sam Trenholme (<A href=https://www.samiam.org/>https://www.samiam.org</a>)
is responsible for this example document.
```

# Using EJ

EJ is a HTML-like format which has the following tags.

Comments:

Comments begin with `<!--` and end with `-->`; these comments are removed
before an ej document is converted.  

Tags to put in the header of the document:

HEAD: Marks the beginning of the header; terminated by /HEAD

TH: Placed in the HEAD of the document; this is the arguments to give TH 
when translated to a man page; terminated by /TH; only applies when 
converting ej documents to *ROFF man pages

DTWIDTH: How wide to make DT entries when translating to the man page
         format

TITLE: The title of the document when the document is translated to HTML

BODYFLAGS: Flags given to the BODY tag when this document is translated
           to HTML

meta HTTP-EQUIV="Content-Type" CONTENT="text/html; charset=utf-8": Mandatory;
all documents must be encoded as utf-8 documents.

Tags to put in the body of the document:

BODY: Marks the beginning of the body; terminated by /BODY

H1: Same as in HTML; becomes .SH when translated in to a man page; placed 
in BODY of message; terminated by /H1

H2: Heading level 2; becomes a fairly complex series of roff code when
    translated to man page format

B: Bold text; terminated by /B

I: Italic text; terminated by /I

UL: Start a bulleted list; terminated by /UL

LI: Bulleted list item.  Please minimize tag use in bulleted lists,
    using only B and I tags.

OL: Rendered as a bulleted list with ej2txt and ej2man.  Rendered as
    numbered lists with ej2html.

PRE: Unformatted text follows; this tag is terminated by /PRE
     Note that HTML tags are shown as is in PRE blocks; unlike HTML,
     tags have no meta-significance in a PRE block but are instead 
     shown as the raw tag.  Likewise, < and > can be in PRE blocks.
     PRE tags should be put on a line by themselves.

INCLUDE "filename": Embed the listed filename as the next section of the doc

BLOCKQUOTE: Move the following text over; terminated by /BLOCKQUOTE

P: Indicates a new paragraph

A: Indicates an anchor; same as a HTML anchor; terminated with /A

TT: Indicates fix-point text (only rendered in HTML pages)

TABLE: Signifies the beginning of a basic three-column table; terminated
       with /TABLE

TD: Signifies the start of a single table cell

TR: Signifies the start of a new row with the table

BR: Line break

DL: Start a definition list

DT: Start to describe the item to define; can be closed by /DT
    (to work around a bug in the Konqueror web browser)

DD: Start to define the item just declared with the DL tag; can be closed
    by /DD 

HR: This is used to split up sections of the document

HIBIT: This was a special tag used to indicate a section that needs
       hi-bit (non-ASCII Unicode) characters.  Now that UTF-8 is universal,
       this tag is no longer used.

NOFMT: This is a special tag used to indicate to not attempt to make
       lines under 72 columns wide when generating text and *ROFF 
       documents.  This should be placed in the head of documents if
       the language is not a Latin/Greek/Cyrillic lanaguage (i.e. a
       language where a given UTF-8 code point does not have a fixed
       width).  This tag must be placed in the head section of the
       document.

# HTML entities

HTML entity support is here, with limitations.  In theory, `&lt;`, `&gt;`,
and `&amp;` will be kept as is in PRE blocks and otherwise converted
in to the appropriate character.  In practice, HTML entities should be
used very sparingly and should always be surrounded by white space
to work correctly.

# Status

The EJ tools are *not* a general purpose text formatting language.
They are a convenient way for me to generate HTML pages, man pages,
and text documents from one source of truth.  In particular, there
are a lot of corner cases the EJ tools do not handle well.

# Why not Docbook?

The reason why this is used instead of something like docbook is 
because it’s a simple language which can be easily changed should the
MaraDNS documentation need to add another formatting feature for its
documents.  For example, the MaraDNS documents have a single 3-column
table in them, so the TABLE document is here to support that one
single table.

Another reason to use these tools instead of, say, docbook is because
MaraDNS’s documentation does not need to adapt to a moving target to
render correctly.  To quote [one anti-Docbook rant](https://undeadly.org/cgi?action=article&sid=20190419101505),
“DocBook 4.5 documents almost certainly misformat or even abort
formatting with fatal parsing errors with a DocBook 5.1 formatter”.  
Since MaraDNS is an open source project being developed part time,
it is a waste of limited open source resources to revise all of the
documents every time the docbook format is revised.  When I wrote the
original version of the EJ tools, in 2002, Docbook was working on
version 4.2 of their specification.  As I type this, Docbook is at 
version 5.1, which is *not* compatible with the Docbook 4.2 which 
existed when I wrote the initial version of the EJ documents.

Here in the 2020s, the EJ documents render the same way they rendered
back in 2002.  By keeping the document rendering pipeline completely
in house, using *only* tools included with MaraDNS to render them,
the documents can last a long time without needing maintenance when 
the underlying file format used changes.

This is also why the EJ tools have been, for the 2020s, rewritten in Lua
5.1 (a version of which is now included with MaraDNS) instead of using
Perl.  This way, there’s one less moving target for MaraDNS to need
to keep up with.  To Perl’s credit, since the Perl6 project somewhat
sputtered, the Perl scripts I was using to process EJ documents in 2002
run in 2022 without issue.  It’s Python which broke compatibility
and required scripts to be rewritten, and while Perl is not Python,
the entire experience of having to revise MaraDNS’s one Python script
in 2019 because of the deprecation of Python2 has left a bad taste in
my mouth. 

MaraDNS is, with the exception of one Python script, a setup which
only needs a POSIX system, a POSIX-compatible `make` (I am glad I 
ignored the users who asked me to use Autoconf, since that is a 
very unstable moving target), and a C compiler (both GCC and Clang work)
with 8/16/32/64 bit sized int support to run.
