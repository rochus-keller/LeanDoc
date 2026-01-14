<!--  This file may be used under the terms of the GNU General Public -->
<!--  License (GPL) versions 2.0 or 3.0 as published by the Free Software -->
<!--  Foundation, see http://www.gnu.org/copyleft/gpl.html for more information -->

# LeanDoc: A Semantic Document Language in the tradition of AsciiDoc

**Version:** 2026-01-13, work in progress
**Author:** me@rochus-keller.ch

---

## Table of Contents

1. [Introduction](#introduction)
2. [Design Principles](#design-principles)
3. [Lexical Structure](#lexical-structure)
4. [Grammar Specification (EBNF)](#grammar-specification-ebnf)
5. [Feature Descriptions](#feature-descriptions)
6. [AsciiDoc Feature Mapping](#asciidoc-feature-mapping)

---

## 1. Introduction

LeanDoc is a semantic document markup language designed to cover the cover the essential subset of of AsciiDoc while being parsable by a single recursive descent parser. The language prioritizes minimal boilerplate syntax, uniform constructs, and ease of learning for the most commonly used features.

Each LeanDoc document is a valid AsciiDoc document (but not vice versa).

This specification serves as both a reference for language users and a blueprint for parser implementers.

### Design Goals

- **Complete AsciiDoc coverage**: All AsciiDoc features are representable
- **Single-pass parsing**: No integrated sub-parsers required
- **Minimal boilerplate**: Less syntax burden for common operations, benefiting frequently-used features
- **Uniform syntax**: Consistent patterns across similar features, patterns reducing learning curve
- **Progressive complexity**: Simple tasks are simple; complex tasks are possible
- **Line-oriented design** enabling efficient parsing

---

## 2. Design Principles

### Principle 1: Line-Oriented Parsing
Most constructs begin at line boundaries, making recursive descent parsing straightforward without lookahead beyond the current line start.

### Principle 2: Contextual Blocks
Block delimiters use context-sensitive markers that change meaning based on position (line start vs. inline), eliminating ambiguity without complex lookahead.

### Principle 3: Uniform Attribute Syntax
All metadata (IDs, roles, options) use a single bracket notation `[...]` with consistent parsing rules across block and inline contexts.

### Principle 4: Explicit Over Implicit
Where AsciiDoc infers context (e.g., distinguishing quote blocks from other blocks), LeanDoc uses explicit markers or attributes, reducing parser complexity.

### Principle 5: Frequency-Weighted Syntax
Most common features (paragraphs, basic formatting, lists, links) require minimal syntax. Less common features (conditional compilation, complex tables) accept higher syntax overhead.

---

## 3. Lexical Structure

### 3.1 Character Classes

```ebnf
(* Basic character classes *)
ALPHA = "a".."z" | "A".."Z" ;
DIGIT = "0".."9" ;
WHITESPACE = " " | "\t" ;
NEWLINE = "\n" | "\r\n" | "\r" ;
ANY_CHAR = ? any Unicode character ? ;

(* Line-level tokens *)
LINE_START = ? start of line position ? ;
LINE_END = NEWLINE | EOF ;
BLANK_LINE = WHITESPACE* LINE_END ;
EOF = ? end of file ? ;

(* Delimiter characters *)
EQUALS = "=" ;
DASH = "-" ;
STAR = "*" ;
DOT = "." ;
COLON = ":" ;
PIPE = "|" ;
SLASH = "/" ;
PLUS = "+" ;
TILDE = "~" ;
BACKTICK = "`" ;
BRACKET_OPEN = "[" ;
BRACKET_CLOSE = "]" ;
PAREN_OPEN = "(" ;
PAREN_CLOSE = ")" ;
BRACE_OPEN = "{" ;
BRACE_CLOSE = "}" ;
ANGLE_OPEN = "<" ;
ANGLE_CLOSE = ">" ;
HASH = "#" ;
UNDERSCORE = "_" ;
CARET = "^" ;
BACKSLASH = "\\" ;
```

### 3.2 Lexical Tokens

```ebnf
(* Text content *)
TEXT_CHAR = ANY_CHAR - ( NEWLINE | "[" | "]" | "*" | "_" | "`" | "^" | "~" | "#" | "{" | "}" ) ;
TEXT_RUN = TEXT_CHAR+ ;

(* Identifiers *)
IDENTIFIER = ( ALPHA | "_" ) ( ALPHA | DIGIT | "_" | "-" )* ;

(* Strings *)
STRING_LITERAL = '"' ( ANY_CHAR - '"' | '\"' )* '"' ;

(* Operators *)
ATTR_SEPARATOR = "," ;
ASSIGN = "=" ;
```

---

## 4. Grammar Specification (EBNF)

### 4.1 Document Structure

```ebnf
(* Top-level document *)
Document = [ DocumentHeader ] DocumentBody ;

DocumentHeader = DocumentTitle 
                 [ AuthorLine ]
                 [ RevisionLine ]
                 AttributeLine* ;

DocumentTitle = LINE_START EQUALS WHITESPACE+ TEXT_RUN LINE_END ;

AuthorLine = LINE_START IDENTIFIER ( WHITESPACE+ IDENTIFIER )* 
             [ WHITESPACE+ "<" EMAIL ">" ] LINE_END ;

EMAIL = IDENTIFIER "@" IDENTIFIER ( "." IDENTIFIER )+ ;

RevisionLine = LINE_START "v" VERSION_NUMBER 
               [ "," WHITESPACE* DATE ] LINE_END ;

VERSION_NUMBER = DIGIT+ ( "." DIGIT+ )* ;
DATE = DIGIT DIGIT DIGIT DIGIT "-" DIGIT DIGIT "-" DIGIT DIGIT ;

AttributeLine = LINE_START COLON IDENTIFIER COLON 
                 [ WHITESPACE+ AttributeValue ] LINE_END ;

AttributeValue = TEXT_RUN | BLANK ;

DocumentBody = ( Block | BLANK_LINE )* ;
```

### 4.2 Blocks

```ebnf
Block = [ BlockMetadata ] 
        ( Section 
        | AdmonitionParagraph
        | Paragraph 
        | DelimitedBlock 
        | List 
        | Table 
        | BlockMacro 
        | Directive
        | ThematicBreak 
        | Comment ) ;

BlockMetadata = [ BlockAnchor ] 
                [ BlockAttributes ]
                [ BlockTitle ] ;

BlockAnchor = LINE_START "[[" IDENTIFIER [ "," WHITESPACE* TEXT_RUN ] "]]" LINE_END ;

BlockAttributes = LINE_START "[" AttributeList "]" LINE_END ;

AttributeList = [ AttributeEntry { "," WHITESPACE* AttributeEntry } ] ;

AttributeEntry = IDENTIFIER [ "=" ( IDENTIFIER | STRING_LITERAL ) ] ;

BlockTitle = LINE_START "." TEXT_RUN LINE_END ;
```

### 4.3 Sections

```ebnf
Section = SectionTitle SectionBody ;

SectionTitle = LINE_START EQUALS{1..6} WHITESPACE+ TEXT_RUN LINE_END ;

SectionBody = ( Block | BLANK_LINE )* ;
```

### 4.4 Paragraphs

```ebnf
Paragraph = LiteralParagraph | NormalParagraph ;
NormalParagraph = ParagraphLine+ ;

ParagraphLine = LINE_START InlineContent LINE_END ;

(* Special paragraph types *)
LiteralParagraph = LINE_START WHITESPACE+ TEXT_RUN LINE_END 
                   ( LINE_START WHITESPACE+ TEXT_RUN LINE_END )* ;

AdmonitionParagraph = LINE_START AdmonitionLabel COLON 
                      WHITESPACE+ InlineContent LINE_END ;

AdmonitionLabel = "NOTE" | "TIP" | "IMPORTANT" | "CAUTION" | "WARNING" ;
```

### 4.5 Delimited Blocks

```ebnf
DelimitedBlock = ListingBlock 
               | LiteralBlock 
               | QuoteBlock 
               | ExampleBlock 
               | SidebarBlock 
               | OpenBlock 
               | PassthroughBlock 
               | CommentBlock
               | StemBlock ;

(* Common pattern for delimited blocks *)
ListingBlock = LINE_START DASH{4} LINE_END 
               BlockContent 
               LINE_START DASH{4} LINE_END ;

LiteralBlock = LINE_START DOT{4} LINE_END 
               VerbatimContent 
               LINE_START DOT{4} LINE_END ;

QuoteBlock = LINE_START UNDERSCORE{4} LINE_END 
             BlockContent 
             LINE_START UNDERSCORE{4} LINE_END ;

ExampleBlock = LINE_START EQUALS{4} LINE_END 
               BlockContent 
               LINE_START EQUALS{4} LINE_END ;

SidebarBlock = LINE_START STAR{4} LINE_END 
               BlockContent 
               LINE_START STAR{4} LINE_END ;

OpenBlock = LINE_START DASH{2} LINE_END 
            BlockContent 
            LINE_START DASH{2} LINE_END ;

PassthroughBlock = LINE_START PLUS{4} LINE_END 
                   RawContent 
                   LINE_START PLUS{4} LINE_END ;

CommentBlock = LINE_START SLASH{4} LINE_END 
               ANY_CHAR* 
               LINE_START SLASH{4} LINE_END ;

StemBlock = LINE_START "[" "stem" "]" LINE_END
            LINE_START PLUS{4} LINE_END 
            MathContent 
            LINE_START PLUS{4} LINE_END ;

BlockContent = ( Paragraph | List | Table | DelimitedBlock | BLANK_LINE )* ;
VerbatimContent = ( TEXT_RUN LINE_END )* ;
RawContent = ( ANY_CHAR - ( LINE_START PLUS{4} ) )* ;
MathContent = ( ANY_CHAR - ( LINE_START PLUS{4} ) )* ;
```

### 4.6 Lists

```ebnf
List = UnorderedList | OrderedList | DescriptionList ;

UnorderedList = UnorderedListItem+ ;

UnorderedListItem = LINE_START STAR{1..6} WHITESPACE+ 
                    InlineContent LINE_END 
                    [ ListItemContinuation ]
                    [ NestedList ] ;

OrderedList = OrderedListItem+ ;

OrderedListItem = LINE_START DOT{1..6} WHITESPACE+ 
                  InlineContent LINE_END 
                  [ ListItemContinuation ]
                  [ NestedList ] ;

DescriptionList = DescriptionListEntry+ ;

DescriptionListEntry = DescriptionTerm DescriptionDefinition ;

DescriptionTerm = LINE_START TEXT_RUN COLON{2,} LINE_END ;

DescriptionDefinition = [ LINE_START InlineContent LINE_END ]
                        [ ListItemContinuation ] ;

ListItemContinuation = BLANK_LINE LINE_START PLUS LINE_END BLANK_LINE
                       ( Paragraph | DelimitedBlock ) ;

NestedList = BLANK_LINE List ;
```

### 4.7 Tables

```ebnf
Table = [ TableAttributes ] TableDelimiterOpen TableContent TableDelimiterClose ;

TableAttributes = LINE_START "[" TableAttributeList "]" LINE_END ;

TableAttributeList = TableAttributeEntry { "," WHITESPACE* TableAttributeEntry } ;

TableAttributeEntry = ( "cols" "=" STRING_LITERAL )
                    | ( "options" "=" STRING_LITERAL ) 
                    | ( "title" "=" STRING_LITERAL ) ;
               
TableDelimiterOpen = LINE_START PIPE EQUALS{3} LINE_END ;

TableDelimiterClose = LINE_START PIPE EQUALS{3} LINE_END ;

TableContent = ( TableRow | BLANK_LINE )* ;

TableRow = LINE_START (PIPE TableCell)+ LINE_END ;

TableCell = [ CellSpec PIPE ] InlineContent ;

CellSpec = [ ColSpan "+" ] [ Alignment ] ;

ColSpan = DIGIT+ ;

Alignment = "<" | "^" | ">" ;
```

### 4.8 Inline Elements

```ebnf
InlineContent = ( InlineElement | TEXT_RUN | WHITESPACE )* ;

InlineElement = Bold 
              | Italic 
              | Monospace 
              | Superscript 
              | Subscript 
              | Highlight
              | CustomRole
              | Link 
              | Image 
              | InlineAnchor 
              | CrossReference 
              | AttributeReference 
              | InlineMacro
              | InlineStem
              | Passthrough
              | InlineStem
              | LineBreak ;

(* Constrained (word boundary) formatting *)
Bold = STAR TEXT_RUN STAR ;
Italic = UNDERSCORE TEXT_RUN UNDERSCORE ;
Monospace = BACKTICK TEXT_RUN BACKTICK ;

(* Unconstrained (anywhere) formatting *)
UnconstrainedBold = STAR{2} InlineContent STAR{2} ;
UnconstrainedItalic = UNDERSCORE{2} InlineContent UNDERSCORE{2} ;
UnconstrainedMonospace = BACKTICK{2} InlineContent BACKTICK{2} ;

Superscript = CARET TEXT_RUN CARET ;
Subscript = TILDE TEXT_RUN TILDE ;
Highlight = HASH TEXT_RUN HASH ;

CustomRole = "[" "." IDENTIFIER "]" HASH InlineContent HASH ;

Link = URLAutoLink | URLWithText | EmailLink | InternalLink ;

URLAutoLink = URL_SCHEME URL_PATH ;
URL_SCHEME = ( "http" | "https" | "ftp" | "irc" | "mailto" ) ":" ;
URL_PATH = ( ANY_CHAR - ( WHITESPACE | NEWLINE | "[" | "]" ) )+ ;

URLWithText = URL_SCHEME URL_PATH "[" InlineContent [ "," LinkAttributes ] "]" ;
LinkAttributes = AttributeEntry { "," WHITESPACE* AttributeEntry } ;

EmailLink = IDENTIFIER "@" IDENTIFIER ( "." IDENTIFIER )+ ;

InternalLink = "link:" IDENTIFIER "[" InlineContent "]" ;

Image = "image:" ImagePath "[" [ ImageAttributes ] "]" ;

ImagePath = ( ANY_CHAR - ( "[" | "]" | WHITESPACE ) )+ ;
ImageAttributes = [ InlineContent ] { "," AttributeEntry } ;

InlineAnchor = "[[" IDENTIFIER [ "," InlineContent ] "]]" 
             | "[" HASH IDENTIFIER "]" ;

CrossReference = "<<" IDENTIFIER [ "," InlineContent ] ">>" 
               | "xref:" IDENTIFIER "[" [ InlineContent ] "]" ;

AttributeReference = BRACE_OPEN IDENTIFIER BRACE_CLOSE ;

InlineMacro = MacroName COLON "[" [ MacroAttributes ] "]" 
            | MacroName COLON MacroTarget "[" [ MacroAttributes ] "]" ;

MacroName = "kbd" | "btn" | "menu" | "pass" | "footnote" 
          | "stem" | IDENTIFIER ;

MacroTarget = ( ANY_CHAR - ( "[" | "]" ) )+ ;
MacroAttributes = InlineContent { "," WHITESPACE* InlineContent } ;

InlineStem = "stem:" "[" MathExpression "]" ;
MathExpression = ( ANY_CHAR - "]" )+ ;

Passthrough = PassthroughTriple | PassthroughDouble | PassthroughPlus ;
PassthroughTriple = PLUS{3} InlineContent PLUS{3} ;
PassthroughDouble = PLUS{2} InlineContent PLUS{2} ;
PassthroughPlus = PLUS InlineContent PLUS ;

LineBreak = WHITESPACE PLUS LINE_END ;
```

### 4.9 Block Macros

```ebnf
BlockMacro = IncludeMacro 
           | CustomBlockMacro ;

IncludeMacro = LINE_START "include::" IncludePath "[" [ IncludeAttributes ] "]" LINE_END ;
IncludePath = ( ANY_CHAR - ( "[" | "]" ) )+ ;
IncludeAttributes = AttributeEntry { "," WHITESPACE* AttributeEntry } ;

CustomBlockMacro = LINE_START IDENTIFIER "::" MacroTarget "[" [ MacroAttributes ] "]" LINE_END ;
```

### 4.10 Preprocessor Directives

```ebnf
Directive = ConditionalDirective | IncludeDirective ;

ConditionalDirective = IfdefDirective | IfndefDirective ;

IfdefDirective = LINE_START "ifdef::" AttributeCondition "[" [ InlineContent ] "]" LINE_END
                 [ DocumentBody ]
                 LINE_START "endif::" [ AttributeCondition ] "[" "]" LINE_END ;

IfndefDirective = LINE_START "ifndef::" AttributeCondition "[" [ InlineContent ] "]" LINE_END
                  [ DocumentBody ]
                  LINE_START "endif::" [ AttributeCondition ] "[" "]" LINE_END ;

AttributeCondition = IDENTIFIER { "," IDENTIFIER } ;

```

### 4.11 Comments and Breaks

```ebnf
Comment = LineComment | CommentBlock ;

LineComment = LINE_START SLASH{2} ANY_CHAR* LINE_END ;

ThematicBreak = LINE_START "'''" LINE_END 
              | LINE_START DASH{3} LINE_END
              | LINE_START STAR{3} LINE_END ;

```

### 4.12 Special Sections

```ebnf
SpecialSection = BibliographySection 
               | GlossarySection 
               | IndexSection 
               | AbstractSection
               | AppendixSection ;

BibliographySection = "[" "bibliography" "]" LINE_END Section ;

GlossarySection = "[" "glossary" "]" LINE_END Section ;

IndexSection = "[" "index" "]" LINE_END Section ;

AbstractSection = "[" "abstract" "]" LINE_END Section ;

AppendixSection = "[" "appendix" "]" LINE_END Section ;

BibliographyEntry = LINE_START STAR WHITESPACE+ 
                    "[" "[" "[" IDENTIFIER "]" "]" "]" 
                    WHITESPACE+ InlineContent LINE_END ;
```

---

## 5. Feature Descriptions

### 5.1 Document Structure Features

#### 5.1.1 Document Title
**Syntax:** `= Document Title`

**Description:** The document title appears as the first line of content, marked with a single equals sign followed by whitespace and the title text. This is the only level-0 heading in article-type documents.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
= Introduction to Compiler Design
```

#### 5.1.2 Author Information
**Syntax:** `Author Name <email@example.org>`

**Description:** Follows the document title. Can include multiple authors separated by semicolons. Email is optional.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
= Document Title
John Doe <john@example.org>; Jane Smith <jane@example.org>
```

#### 5.1.3 Revision Information
**Syntax:** `v0.1, 2025-12-30`

**Description:** Version number and optional date following author line.

**AsciiDoc Equivalent:** Identical syntax.

#### 5.1.4 Document Attributes
**Syntax:** `:attribute-name: value`

**Description:** Document-level configuration and variables. Set in header or anywhere in document. Referenced via `{attribute-name}`.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
:toc:
:numbered:
:source-highlighter: pygments
:company: Acme Corp

Welcome to {company}!
```

#### 5.1.5 Sections
**Syntax:** `==` to `======` (2 to 6 equals for levels 1-5)

**Description:** Hierarchical document structure. Number of equals signs determines nesting level.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
== Chapter 1

=== Section 1.1

==== Subsection 1.1.1

== Chapter 2
```

#### 5.1.6 Discrete Headings
**Syntax:** `[discrete]` attribute before heading

**Description:** Headings that don't participate in document hierarchy or TOC.

**AsciiDoc Equivalent:** Identical functionality.

**Example:**
```
[discrete]
=== A Standalone Heading

This heading is not part of the section hierarchy.
```

---

### 5.2 Paragraph Features

#### 5.2.1 Normal Paragraph
**Syntax:** Consecutive non-blank lines

**Description:** Default text block. Separated from other blocks by blank lines. Line breaks within paragraphs are normalized to spaces.

**AsciiDoc Equivalent:** Identical behavior.

**Example:**
```
This is a paragraph that spans
multiple lines but will be rendered
as continuous flowing text.

This is a second paragraph.
```

#### 5.2.2 Literal Paragraph
**Syntax:** Lines indented by at least one space

**Description:** Preserves exact spacing and line breaks. Rendered in monospace font.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
 This is literal text.
   It preserves  spaces and
   line breaks.
```

#### 5.2.3 Hard Line Breaks
**Syntax:** Space + `+` at line end

**Description:** Forces line break in output without starting new paragraph.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
First line +
Second line +
Third line
```

#### 5.2.4 Lead Paragraph
**Syntax:** `[.lead]` attribute before paragraph

**Description:** Styled as introductory paragraph with larger or distinct formatting.

**AsciiDoc Equivalent:** Identical functionality.

**Example:**
```
[.lead]
This is the opening paragraph with special emphasis.
```

#### 5.2.5 Admonition Paragraphs
**Syntax:** `NOTE:`, `TIP:`, `IMPORTANT:`, `CAUTION:`, `WARNING:` prefix

**Description:** Single-paragraph callouts for special attention. Label must be uppercase followed by colon.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
NOTE: This is important information for the reader.

WARNING: Danger ahead! Proceed with caution.
```

---

### 5.3 Text Formatting Features

#### 5.3.1 Bold Text
**Syntax:** `*text*` (constrained), `**text**` (unconstrained)

**Description:** Strong emphasis. Constrained form requires word boundaries; unconstrained works anywhere.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
This is *bold* text.
**B**old within a word.
```

#### 5.3.2 Italic Text
**Syntax:** `_text_` (constrained), `__text__` (unconstrained)

**Description:** Emphasis. Constrained form requires word boundaries; unconstrained works anywhere.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
This is _italic_ text.
__I__talic within a word.
```

#### 5.3.3 Monospace Text
**Syntax:** `` `text` `` (constrained), ``` ``text`` ``` (unconstrained)

**Description:** Fixed-width font for code or literals. Constrained form requires word boundaries.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
Use the `git commit` command.
Use ``Object``s in Java.
```

#### 5.3.4 Superscript and Subscript
**Syntax:** `^superscript^`, `~subscript~`

**Description:** Raised and lowered text for mathematical or chemical notation.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
E = mc^2^
H~2~O
```

#### 5.3.5 Highlighting
**Syntax:** `#highlighted text#`

**Description:** Marked or highlighted text (often rendered with yellow background).

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
Remember to #save your work# frequently.
```

#### 5.3.6 Custom Roles
**Syntax:** `[.rolename]#text#`

**Description:** Apply custom CSS classes or semantic markup to inline text.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
[.underline]#This text is underlined#
[.red]#This text is red#
```

#### 5.3.7 Smart Quotes
**Syntax:** `"`double quotes`"`, `'`single quotes`'`

**Description:** Converts straight quotes to typographic curved quotes.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
He said, "`Hello, world!`"
It's O'Brien's book.
```

---

### 5.4 List Features

#### 5.4.1 Unordered Lists
**Syntax:** `*` prefix (1-6 levels using `*`, `**`, `***`, etc.)

**Description:** Bulleted lists with nesting support. Nesting level determined by number of asterisks.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
* First item
* Second item
** Nested item
** Another nested item
*** Deeper nesting
* Third item
```

#### 5.4.2 Ordered Lists
**Syntax:** `.` prefix (1-6 levels using `.`, `..`, `...`, etc.)

**Description:** Numbered lists with automatic numbering and nesting support.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
. First step
. Second step
.. Sub-step A
.. Sub-step B
. Third step
```

#### 5.4.3 Checklist Items
**Syntax:** `* [*]`, `* [x]`, `* [ ]`

**Description:** Task lists with checked and unchecked states.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
* [*] Completed task
* [x] Also completed
* [ ] Not yet done
* Regular list item
```

#### 5.4.4 Description Lists
**Syntax:** `term::` followed by definition

**Description:** Term-definition pairs (also called labeled lists or definition lists). Multiple colons increase nesting.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
CPU:: Central Processing Unit
RAM:: Random Access Memory
  A type of volatile memory.

Compiler::
  A program that translates source code
  into machine code.
```

#### 5.4.5 Question and Answer Lists

Not supported. Use plain description lists or titles instead.

#### 5.4.6 List Continuation
**Syntax:** `+` on line by itself between list item and attached block

**Description:** Allows attaching paragraphs or blocks to list items.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
* List item with additional content
+
This paragraph is part of the list item.
+
----
Code block also attached.
----

* Next list item
```

---

### 5.5 Delimited Block Features

#### 5.5.1 Listing Block
**Syntax:** `----` delimiters

**Description:** Verbatim preformatted block, typically used for code or command output.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
----
def hello():
    print("Hello, world!")
----
```

#### 5.5.2 Source Code Block
**Syntax:** `[source,language]` attribute followed by `----` delimiters

**Description:** Code block with syntax highlighting for specified language.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
[source,python]
----
def factorial(n):
    return 1 if n == 0 else n * factorial(n-1)
----
```

#### 5.5.3 Literal Block
**Syntax:** `....` delimiters

**Description:** Like listing block but with different semantic meaning (often rendered identically).

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
....
Error: File not found
Exit code: 1
....
```

#### 5.5.4 Quote Block
**Syntax:** `____` delimiters with optional `[quote,attribution,source]` attribute

**Description:** Blockquote with optional attribution and citation.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
[quote,Alan Perlis]
____
A language that doesn't affect the way you think about
programming is not worth knowing.
____
```

#### 5.5.5 Example Block
**Syntax:** `====` delimiters

**Description:** Block for examples, often rendered with distinct styling.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
====
This is an example that might contain
multiple paragraphs and other blocks.

* List item
* Another item
====
```

#### 5.5.6 Sidebar Block
**Syntax:** `****` delimiters

**Description:** Supplementary content displayed alongside main text.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
****
.Related Information
This sidebar contains additional context
that supplements the main text.
****
```

#### 5.5.7 Open Block
**Syntax:** `--` delimiters

**Description:** Generic block container that can masquerade as other block types.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
[source]
--
This open block is styled as source code.
--
```

#### 5.5.8 Passthrough Block
This feature is not supported. For math. formulae, use latexmath: instead.

#### 5.5.9 Admonition Blocks
**Syntax:** `[NOTE]`, `[TIP]`, etc. attribute before block

**Description:** Multi-paragraph admonitions using any delimited block.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
[IMPORTANT]
====
This is a complex admonition with:

* Multiple paragraphs
* Lists
* Other nested content
====
```

---

### 5.6 Table Features

#### 5.6.1 Basic Table
**Syntax:** `|===` delimiters with `|` cell separators

**Description:** Simple table with automatic column detection.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
|===
|Column 1 |Column 2

|Cell 1,1 |Cell 1,2
|Cell 2,1 |Cell 2,2
|===
```

#### 5.6.2 Table with Header
**Syntax:** First row promoted to header when followed by blank line, or `[options="header"]` attribute

**Description:** Tables with distinguished header row.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
[options="header"]
|===
|Name |Age |City

|Alice |30 |New York
|Bob |25 |Boston
|===
```

#### 5.6.3 Column Specifications
**Syntax:** `[cols="spec"]` attribute

**Description:** Define number, width, and alignment of columns.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
[cols="1,2,3"]
|===
|Narrow |Medium |Wide

|A |B |C
|===

[cols="<,^,>"]
|===
|Left |Center |Right

|A |B |C
|===
```

#### 5.6.4 Cell Spanning
**Syntax:** `2+|` (column span)

**Description:** Cells that span multiple rows or columns.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
|===
|Normal cell |2+|Spans 2 columns

.2+|Spans 2 rows |Cell |Cell
|Cell |Cell
|===
```

#### 5.6.5 Cell Alignment and Styling
**Syntax:** `<|` (left), `^|` (center), `>|` (right)

**Description:** Control cell content alignment and processing.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
|===
|<| Left aligned
|^| Center aligned
|>| Right aligned
|===
```

#### 5.6.6 CSV Tables
Not supported.

---

### 5.7 Link and Cross-Reference Features

#### 5.7.1 URL Auto-linking
**Syntax:** `https://example.org`

**Description:** URLs are automatically converted to clickable links.

**AsciiDoc Equivalent:** Identical behavior.

**Example:**
```
Visit https://example.org for more information.
```

#### 5.7.2 URL with Link Text
**Syntax:** `https://example.org[Link Text]`

**Description:** Hyperlink with custom display text.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
Check out https://asciidoc.org[AsciiDoc] for documentation.
```

#### 5.7.3 Link Attributes
**Syntax:** `https://example.org[Text,role=external,window=_blank]`

**Description:** Add attributes like target window or CSS classes to links.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
https://example.org[External Site,window=_blank]
https://example.org[External Site^] // shorthand for window=_blank
```

#### 5.7.4 Email Links
**Syntax:** `user@example.org` or `mailto:user@example.org[Link Text]`

**Description:** Email addresses auto-linked or with custom text.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
Contact support@example.org for help.
mailto:sales@example.org[Contact Sales]
```

#### 5.7.5 Internal Anchors
**Syntax:** `[[anchorid]]` or `[#anchorid]`

**Description:** Define targets for cross-references within document.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
[[important-section]]
== Important Section

Text with [#inline-anchor]#inline anchor#.
```

#### 5.7.6 Cross-References
**Syntax:** `<<anchorid>>` or `<<anchorid,Custom Text>>` or `xref:anchorid[Text]`

**Description:** Link to anchors within same document.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
See <<important-section>> for details.
See <<important-section,the important section>> for details.
See xref:important-section[important details].
```

#### 5.7.7 Inter-document Cross-References
**Syntax:** `xref:other-doc.adoc#anchorid[Text]`

**Description:** Link to sections in other documents.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
See xref:chapter2.adoc#section2-1[Chapter 2, Section 1].
Refer to xref:appendix.adoc[the appendix].
```

---

### 5.8 Image, Audio, and Video Features

#### 5.8.1 Block Image
No longer supported. Instead use a paragraph which only contains one inline image. 

#### 5.8.2 Inline Image
**Syntax:** `image:filename.jpg[]`

**Description:** Image embedded within text flow.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
Click the image:button.png[] button to continue.

image:file.jpg[alt text,width,height,opts]
```

#### 5.8.3 Image Attributes
**Syntax:** `image:file.jpg[alt text,width,height,opts]`

**Description:** Control image display with alt text, dimensions, alignment, etc.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
image:photo.jpg[A beautiful sunset,400,300,align=center]
image:logo.png[Company Logo,link=https://example.com]
```

#### 5.8.4 Audio
Not supported.

#### 5.8.5 Video
Not supported.


#### 5.8.6 YouTube/Vimeo Embedding
Not supported.

---

### 5.9 Include and Macro Features

#### 5.9.1 Include Directive
**Syntax:** `include::filename.adoc[]`

**Description:** Include content from external files.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
include::chapter1.adoc[]

include::shared/header.adoc[]
```

#### 5.9.2 Include with Tag Selection
**Syntax:** `include::file.adoc[tag=tagname]`

**Description:** Include only tagged regions from file.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
include::examples.java[tag=main-method]

// In examples.java:
// tag::main-method[]
public static void main(String[] args) {
    System.out.println("Hello");
}
// end::main-method[]
```

#### 5.9.3 Include with Line Ranges
**Syntax:** `include::file.txt[lines=5..10]`

**Description:** Include specific line ranges from file.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
include::data.txt[lines=1..5]
include::code.py[lines=10..20;30..40]
```

#### 5.9.4 Keyboard Macro
Not supported.

#### 5.9.5 Button Macro
Not supported.

#### 5.9.6 Menu Macro
Not supported.

#### 5.9.7 Footnote Macro
**Syntax:** `footnote:[text]` or `footnote:id[text]` for reuse

**Description:** Add footnotes with automatic numbering.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
This statement needs clarification.footnote:[Additional details here.]

First reference.footnote:disclaimer[Opinions are my own.]
Second reference.footnote:disclaimer[]
```

#### 5.9.8 Index Term Macros
**Syntax:** `((term))`, `(((term)))`

**Description:** Mark terms for index generation.

**AsciiDoc Equivalent:** only `((term))`, `(((term)))` supported.

**Example:**
```
The ((parser)) analyzes syntax.
Use (((recursive descent))) parsing for LL(k) grammars.
```

---

### 5.10 Mathematical Content Features

#### 5.10.1 Inline STEM
Not supported. Use latexmath: instead.


#### 5.10.2 Block STEM
Not supported. Use a paragraph with latexmath: instead.

#### 5.10.3 LaTeX Math Notation
**Syntax:** `latexmath:[...]` for both inline an block math

**Description:** Use LaTeX/TeX notation for math.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
latexmath:[\int_0^\infty e^{-x^2} dx = \frac{\sqrt{\pi}}{2}]

```

#### 5.10.4 Equation Numbering
This feature is no longer necessary, since formula blocks are automatically detected and referencable
with the normal means.

**Example:**
```
[#pythagorean]
latexmath:[a^2 + b^2 = c^2]

As seen in <<pythagorean>>...
```

---

### 5.11 Preprocessor Directive Features

#### 5.11.1 Ifdef Directive
**Syntax:** `ifdef::attribute[]...endif::[]`

**Description:** Conditionally include content if attribute is defined.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
:production:

ifdef::production[]
This content only appears in production builds.
endif::[]
```

#### 5.11.2 Ifndef Directive
**Syntax:** `ifndef::attribute[]...endif::[]`

**Description:** Conditionally include content if attribute is NOT defined.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
ifndef::hide-internal[]
This is internal documentation.
endif::[]
```

#### 5.11.3 Multiple Attribute Conditions
**Syntax:** `ifdef::attr1,attr2[]` (OR)

Note that AND is not directly supported, but can be achieved by a series of ifdef directives (see example below).

**Description:** Test multiple attributes in single condition.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
ifdef::backend-html5,backend-pdf[]
This appears in HTML or PDF output.
endif::[]

ifdef::attribute-a[]
ifdef::attribute-b[]
Content for A and B
endif::[]
endif::[]
```
### 5.12 Comment Features

#### 5.12.1 Line Comments
**Syntax:** `// comment text`

**Description:** Single-line comments ignored by processor.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
// This is a comment that won't appear in output
This is visible text.
```

#### 5.12.2 Block Comments
**Syntax:** `////` delimiters

**Description:** Multi-line comments ignored by processor.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
////
This entire block is commented out.
It can span multiple lines.
Even include formatting like *bold*.
None of this appears in output.
////
```

---

### 5.13 Break Features

#### 5.13.1 Thematic Break
**Syntax:** `'''` or `---` or `***`

**Description:** Horizontal rule / section separator.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
First section content.

'''

Second section content.
```

#### 5.13.2 Page Break
Not supported.

---

### 5.14 Special Section Features

#### 5.14.1 Bibliography Section
**Syntax:** `[bibliography]` attribute before section

**Description:** Section containing bibliographic references.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
[bibliography]
== References

* [[[taoup]]] Eric S. Raymond. The Art of Unix Programming. Addison-Wesley, 2003.
* [[[gof]]] Gamma et al. Design Patterns. Addison-Wesley, 1994.

In text: See <<taoup>> for details.
```

#### 5.14.2 Glossary Section
**Syntax:** `[glossary]` attribute before section with description list

**Description:** Specialized section for term definitions.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
[glossary]
== Glossary

Parser:: Program component that analyzes syntax.
Lexer:: Component that breaks input into tokens.
```

#### 5.14.3 Index Section
**Syntax:** `[index]` attribute before section

**Description:** Section where index is automatically generated.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
[index]
== Index

// Index entries automatically populated here
```

#### 5.14.4 Abstract Section
**Syntax:** `[abstract]` attribute before section

**Description:** Document abstract or summary.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
[abstract]
== Abstract

This paper presents a new approach to parsing...
```

#### 5.14.5 Appendix Section
**Syntax:** `[appendix]` attribute before section

**Description:** Appendix sections numbered differently (A, B, C...).

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
[appendix]
== Appendix A: Additional Data

[appendix]
== Appendix B: Code Listings
```

---

### 5.15 Attribute and Substitution Features

#### 5.15.1 Attribute References
**Syntax:** `{attribute-name}`

**Description:** Insert value of document attribute.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
:product: LeanDoc Parser
:version: 0.1

Welcome to {product} version {version}!
```

#### 5.15.2 Counter Attributes

Not supported.

#### 5.15.3 Text Replacements
**Syntax:** Automatic conversion of special character sequences

**Description:** Convert `(C)` to ©, `->` to →, `...` to …, etc.

**AsciiDoc Equivalent:** Identical behavior.

**Example:**
```
Copyright (C) 2026
Arrow: -> 
Ellipsis...
```

#### 5.15.4 Escape Substitutions
**Syntax:** Backslash prefix `\`

**Description:** Prevent processing of special syntax.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
\*Not bold\*
\{not-an-attribute}
\https://not-a-link.org
```

#### 5.15.5 Passthrough Substitution

Not supported.

### 5.16 Block Metadata Features

#### 5.16.1 Block ID
**Syntax:** `[[id]]` or `[#id]` before block

**Description:** Assign unique identifier to block for cross-referencing.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
[[important-note]]
NOTE: This is an important note.

See <<important-note>> for details.
```

#### 5.16.2 Block Role
**Syntax:** `[.role-name]` or `[role="role-name"]` before block

**Description:** Apply CSS class or semantic role to block.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
[.lead]
This is introductory text.

[.warning]
This paragraph has custom styling.
```

#### 5.16.3 Block Options
**Syntax:** `[options="option1,option2"]` before block

**Description:** Control block behavior (e.g., header, footer).

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
[options="header,footer"]
|===
|Header
|Content
|Footer
|===

```

#### 5.16.4 Block Title
**Syntax:** `.Title Text` before block

**Description:** Add caption or title to block.

**AsciiDoc Equivalent:** Identical syntax.

**Example:**
```
.Listing 1: Hello World
----
print("Hello, world!")
----

.Table 1: Results
|===
|A |B
|===
```

---

### 5.17 Extension Features
Not supported.

---

## 6. AsciiDoc Feature Mapping

### Complete Feature Coverage Matrix

| AsciiDoc Feature | LeanDoc Syntax | Notes |
|-----------------|--------------|-------|
| **Document Structure** |
| Document title | `= Title` | |
| Author info | `Name <email>` | |
| Revision | `v0.1, 2025-12-30` | |
| Attributes | `:name: value` | |
| Sections | `==` to `======` | |
| Discrete heading | `[discrete]` | |
| TOC | `:toc:` attribute | |
| **Paragraphs** |
| Normal | Consecutive lines | |
| Literal | Indent with space | |
| Line break | `space+plus` | |
| Lead | `[.lead]` | |
| Admonition | `NOTE:` prefix | |
| **Text Formatting** |
| Bold | `*bold*`, `**bold**` | |
| Italic | `_italic_`, `__italic__` | |
| Monospace | `` `mono` ``, ``` ``mono`` ``` | |
| Superscript | `^super^` | |
| Subscript | `~sub~` | |
| Highlight | `#mark#` | |
| Custom role | `[.role]#text#` | |
| **Lists** |
| Unordered | `*` markers | |
| Ordered | `.` markers | |
| Checklist | `[*]`, `[x]`, `[ ]` | |
| Description | `term::` | |
| Q&A | | not supported |
| Continuation | `+` | |
| **Delimited Blocks** |
| Listing | `----` | |
| Literal | `....` | |
| Quote | `____` | |
| Example | `====` | |
| Sidebar | `****` | |
| Open | `--` | |
| Passthrough |  | not supported |
| Comment | `////` | |
| Source code | `[source,lang]` + `----` | |
| **Tables** |
| Basic | `\|===` delimiters | |
| Header | First row or `[options="header"]` | |
| Columns | `[cols="spec"]` | |
| Column span | `2+\|` | |
| Cell align | `<\|`, `^\|`, `>\|` | |
| Cell style | | not supported|
| CSV format | | not supported|
| **Links** |
| URL auto | `https://url` | |
| URL with text | `https://url[text]` | |
| Link attrs | `[text,attr=val]` | |
| Email | `user@domain` | |
| Anchor | `[[id]]`, `[#id]` | |
| Xref | `<<id>>`, `xref:id[]` | |
| Inter-doc xref | `xref:file#id[]` | |
| **Media** |
| Block image |  | substituted |
| Inline image | `image:file[]` | |
| Image attrs | `[alt,width,height]` | |
| Audio |  | not supported|
| Video |  | not supported|
| YouTube/Vimeo |  | not supported|
| **Includes** |
| Include file | `include::file[]` | |
| Tag selection | `[tag=name]` | |
| Line ranges | `[lines=1..10]` | |
| **Macros** |
| Keyboard |  | not supported|
| Button | | not supported|
| Menu | | not supported|
| Footnote | `footnote:[text]` | |
| Index terms | `(((text)))`, `((text))` | |
| **Math** |
| Inline STEM |  | substituted|
| Block STEM |  | substituted|
| Equation numbering |  | substituted|
| LaTeX | `latexmath:` | |
| **Directives** |
| ifdef | `ifdef::attr[]` | |
| ifndef | `ifndef::attr[]` | |
| ifeval |  | not supported|
| Multi-attr | `ifdef::a,b[]` | |
| **Comments** |
| Line | `//` | |
| Block | `////` | |
| **Breaks** |
| Thematic | `'''`, `---`, `***` | |
| Page | | not supported|
| **Special Sections** |
| Bibliography | `[bibliography]` | |
| Glossary | `[glossary]` | |
| Index | `[index]` | |
| Abstract | `[abstract]` | |
| Appendix | `[appendix]` | |
| **Attributes** |
| Reference | `{attr}` | |
| Counter |  | substituted|
| Text replacement | `(C)` → ©, etc. | |
| Escape | `\*not bold\*` | |
| Passthrough | | not supported|
| **Metadata** |
| Block ID | `[[id]]`, `[#id]` | |
| Block role | `[.role]` | |
| Block options | `[options="option"]` | |
| Block title | `.Title` | |
| **Extensions** |
| Block processor |  | not supported|
| Inline macro | | not supported|
| Block macro |  | not supported|
| Tree processor | | not supported|

### Summary

LeanDoc is the essential subset of AsciiDoc while maintaining syntax compatibility. This means:

1. **All LeanDoc documents are valid AsciiDoc documents (but not vice versa)**
2. **The grammar is fully parsable by a single recursive descent parser**
3. **No sub-parsers or context-switching parsers are required**
4. **Most common features require minimal syntax**
5. **Syntax patterns are uniform and consistent**

The key to enabling single-pass recursive descent parsing is the line-oriented nature of the syntax, where block-level constructs are always identifiable from the first character(s) of a line, and inline constructs use distinct marker characters that don't conflict with block-level parsing.

---

## 7. Implementation Notes for Parser Writers

### 7.1 Lexical Analysis

The lexer operates in two modes:

1. **Block Mode** (at line start): Recognizes block delimiters, list markers, section headers, metadata
2. **Inline Mode** (within content): Recognizes inline formatting, links, macros, attribute references

Mode switching occurs naturally at newlines and block boundaries.

### 7.2 Recursive Descent Strategy

```
Document
 ├─ parseDocumentHeader()
 │   ├─ parseDocumentTitle()
 │   ├─ parseAuthorLine()
 │   └─ parseAttributeEntries()
 └─ parseDocumentBody()
     └─ parseBlock()  // Recursive
         ├─ parseSection()  // Recursive for nested sections
         ├─ parseParagraph()
         │   └─ parseInlineContent()  // Handles all inline elements
         ├─ parseDelimitedBlock()
         ├─ parseList()  // Handles nesting
         └─ parseTable()
```

### 7.3 Key Parsing Decisions

1. **Line Start Lookahead**: Most blocks identifiable by first 1-6 characters
2. **Inline Context**: When parsing inline content, markup is processed greedily left-to-right
3. **Block Affinity**: Adjacent blocks of same type require separating blank line
4. **Attribute Scoping**: Block attributes consumed by immediately following block

### 7.4 No Backtracking Required

The grammar is designed as LL(k) with k ≤ 6 (section headers), eliminating the need for backtracking. All parsing decisions can be made with limited lookahead.



