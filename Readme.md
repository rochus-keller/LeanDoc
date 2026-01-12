## Welcome to the LeanDoc project

LeanDoc is a semantic document markup language designed to cover the essential subset of [AsciiDoc](https://asciidoc.org/) 
while being formally specified and parsable by a single recursive descent parser. The language prioritizes minimal boilerplate 
syntax, uniform constructs, and ease of learning for the most commonly used features.

**NOTE** that this project is in an early stage and work-in-progress.

### Why LeanDoc

LeanDoc’s specification is intentionally “very similar” to AsciiDoc; it is an essential subset with full syntax compatibility.  
LeanDoc is specified (and constrained) to be cleanly parsable by a single, line-oriented 
recursive-descent grammar, so it regularizes/limits a few historically “looser” corners to remove ambiguity.
To achieve a "Lean" implementation while maintaining the semantic power of AsciiDoc, features that caused high parsing complexity 
(e.g., non-context-free grammar elements) or are relics of older print-centric workflows were removed.

Each LeanDoc document is a valid AsciiDoc document (but not vice versa).

LeanDoc is easier to parse because it is specified as a formal, line-oriented LL(k) grammar intended to be handled by a single recursive-descent 
parser with bounded lookahead and no backtracking, whereas AsciiDoc’s real-world syntax (especially inline) is largely defined by a substitution 
pipeline and many context-dependent/implementation-defined rules; this is not “just grammar”, but depends on processing phases, enabled substitutions, 
and heuristics that don’t fit a small deterministic CFG-style parser the way LeanDoc is designed to.

Wirth explicitly argued that parsing efficiency for a machine correlates directly with readability for a human. If a programming language is sufficiently 
complex or ambiguous that it requires a sophisticated parser with lookahead, backtracking, or bottom-up analysis, then it will be correspondingly difficult 
for humans to understand, since humans possess far less parsing capability than machines. Therefore, languages should be designed with grammars simple enough 
for single-pass, top-down parsing, ensuring both efficient compilation and human comprehension. If a language is ambiuous for the parser, 
then the outcome for the human is a surprise at best.

While I'm aware of the AsciiDoc specification effort, I think that the issues are symptoms of the language itself rather than just the missing specification. That's why
I decided for a number of deliberate language changes to make it leaner, remove ambiguity and reduce context sensitivity. As a side-effect, the result is also
much easier to specify.

Here are some of the differences to AsciiDoc. For more information see [the specification](./documentation/The_LeanDoc_Language_Specification.md).

#### Fixed delimiter regularity

In the EBNF, several delimited blocks are defined with *exact* delimiter lengths (e.g., listing blocks use `----` as `DASH{4}`, literal blocks use `....` as `DOT{4}`, quote uses `____`, example uses `====`, sidebar uses `****`, passthrough uses `++++`, comment uses `////`, and open blocks use `--` as `DASH{2}`).  If older documents relied on “longer fence lines” (more than 4 characters) for readability or nesting, that’s the kind of major compatibility snag LeanDoc’s stricter grammar would expose.

#### Inline syntax normalization

LeanDoc’s inline grammar cleanly separates “constrained” vs “unconstrained” forms for emphasis/monospace (e.g., `*bold*` vs `**...**`, `_italic_` vs `__...__`, and monospace using backticks ```mono``` / `` ``mono`` ``).  It also reserves plus-sign runs for passthrough variants (`+...+`, `++...++`, `+++...+++`) and `++++` blocks, keeping them as explicit “raw/substitution control” constructs in the grammar.

#### Uniform metadata handling

LeanDoc emphasizes a single bracket-based metadata style (attributes/IDs/roles/options) across contexts (e.g., block attributes are parsed as a bracket list, anchors as `[[id,...]]`, roles as `[.role]`, options as `[%option]`, etc.).  The spec also calls out “Attribute Scoping” as a deterministic rule: block attributes are consumed by the immediately following block (i.e., a parser rule, not a best-effort heuristic).

#### Removed features

Verse blocks, video and audio block macros, page breaks, `ifeval` directive + expressions + operators + values (but `ifdef` and `ifndef` are still available), 

`indexterm:[]`, `indexterm2:[]` (but flow index terms with `((term))` or `(((term)))` are still supported), counters (automatically handled via references),

block images (`image::`, instead use a paragraph with only an inline image), 

stem, eqnums and passtrough (`pass:`, `++++`, instead use `latexmath:` for both inline and blocks),

table row spans and cell style, `[%option]` syntax in favour of `[options="option1,option2"]` syntax, CSV tables,

`[%hardbreaks]` option (use the + break method instead),

include only by singular tag (no complex tag filtering logic like `tags=a;b` or exclusions `tag=!a`),

`ifdef::attr1+attr2[]` AND logic (OR logic still supported), `[qanda]` (just use description lists or titles),

no `kbd:`, `btn:`, `menu:` macros, 

`anchor:anchorid[]` syntax (`[[id]]` and `[#id]` still available).

### Planned features

- [x] Derive a new, lean subset from AsciiDoc with a similar syntax, but parseable and with no ambiguity
- [x] Lexer, parser, AST 
- [ ] Semantic validator, processing includes and conditional compilation
- [x] Typst generator (WIP)
- [ ] Integration with Typos engine

### Status on January 10, 2026

A decently complete version of the specification is available, as well as a working parser which is able to parse the example files. Debugging and corrections
are work-in-progress though.

### Status on January 11, 2026

A first version of the Typst generator is working and able to generate files which compile with Typst 0.14.2 and the resulting PDFs look decently.
The following examples work: simple_article, technical_article2, technical_article3.
I need a processor for ifdef to process technical_article1.

### Additional Credits

**[AsciiDoc](https://asciidoc.org/)** v. 2.0.26 under [MIT](https://github.com/asciidoctor/asciidoctor/blob/main/LICENSE)
