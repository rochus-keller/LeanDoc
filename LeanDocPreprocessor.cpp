/*
* Copyright 2026 Rochus Keller <mailto:me@rochus-keller.ch>
*
* This file is part of the LeanDoc document language project.
*
* The following is the license that applies to this copy of the
* file. For a license to use the file under conditions
* other than those described here, please email to me@rochus-keller.ch.
*
* GNU General Public License Usage
* This file may be used under the terms of the GNU General Public
* License (GPL) versions 2.0 or 3.0 as published by the Free Software
* Foundation and appearing in the file LICENSE.GPL included in
* the packaging of this file. Please review the following information
* to ensure GNU General Public Licensing requirements will be met:
* http://www.fsf.org/licensing/licenses/info/GPLv2.html and
* http://www.gnu.org/copyleft/gpl.html.
*/

#include "LeanDocPreprocessor.h"
#include "LeanDocParser2.h"
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtCore/QRegularExpression>
using namespace LeanDoc;

void Preprocessor::error(int line, const QString& msg)
{
    errors.append(PreprocessorError(line, msg));
}

bool Preprocessor::process(Node* doc)
{
    if( !doc || doc->kind != Node::K_Document)
        return false;
    errors.clear();
    collectDocAttrs(doc);
    processChildren(doc, 0);
    return true;
}

void Preprocessor::collectDocAttrs(Node* doc)
{
    // merge document attributes (:name: value) from doc->kv into dattrs
    QMap<QString,QString>::ConstIterator it = doc->kv.constBegin();
    for( ; it != doc->kv.constEnd(); ++it) {
        if( it.key().startsWith("attr:")) {
            QString name = it.key().mid(5);
            dattrs.insert(name, it.value());
        }
    }
}

void Preprocessor::processChildren(Node* parent, int depth)
{
    int i = 0;
    while( i < parent->children.size()) {
        Node* child = parent->children[i];
        if( !child) { ++i; continue; }

        if( child->kind == Node::K_BlockMacro && child->name == "include") {
            if( resolveInclude(parent, i, depth))
                continue; // index stays same, re-check replaced nodes
            ++i;
        } else if( child->kind == Node::K_Directive &&
                   (child->name == "ifdef" || child->name == "ifndef")) {
            if( evaluateConditional(parent, i))
                continue; // re-check at same index
            ++i;
        } else {
            processNode(child, depth);
            ++i;
        }
    }
}

void Preprocessor::processNode(Node* n, int depth)
{
    substituteAttrRefs(n);

    if( n->kind == Node::K_DelimitedBlock && n->delimKind == Node::DK_Listing &&
            !n->text.isEmpty())
        expandVerbatimIncludes(n, depth);

    if( !n->children.isEmpty())
        processChildren(n, depth);
}

void Preprocessor::substituteAttrRefs(Node* n)
{
    substituteInlineList(n->children);
    substituteInlineList(n->titleChildren);
}

void Preprocessor::substituteInlineList(QList<Node*>& inl)
{
    for( int i = 0; i < inl.size(); ++i) {
        Node* child = inl[i];
        if( !child) continue;

        if( child->kind == Node::K_AttrRef) {
            if( dattrs.contains(child->name)) {
                // replace with text node
                Node* txt = new Node(Node::K_Text);
                txt->pos = child->pos;
                txt->text = dattrs.value(child->name);
                inl[i] = txt;
                delete child;
            }
            // if not defined, leave as-is (validator can warn)
        } else if( !child->children.isEmpty())
            substituteInlineList(child->children);
    }
}

static QMap<QString,QString> parseIncludeAttrs(const QString& attrsStr)
{
    QMap<QString,QString> res;
    const QStringList parts = attrsStr.split(',', Qt::SkipEmptyParts);
    for( int i = 0; i < parts.size(); ++i) {
        const QString p = parts[i].trimmed();
        if( p.isEmpty())
            continue;
        const int eq = p.indexOf('=');
        if( eq > 0)
            res.insert(p.left(eq).trimmed(), p.mid(eq+1).trimmed());
        else
            res.insert(p, QString());
    }
    return res;
}

void Preprocessor::checkIncludeAttrs(const QMap<QString,QString>& attrs, int line)
{
    // closed vocabulary of include:: attributes per spec
    QMap<QString,QString>::ConstIterator it = attrs.constBegin();
    for( ; it != attrs.constEnd(); ++it) {
        const QString& key = it.key();
        if( key != "tag" && key != "tags" && key != "lines" && key != "leveloffset")
            error(line, "unknown include:: attribute '" + key +
                  "'; valid attributes are tag, tags, lines, leveloffset");
    }
    if( attrs.contains("leveloffset")) {
        bool ok = false;
        parseLevelOffset(attrs.value("leveloffset"), &ok);
        if( !ok)
            error(line, "invalid leveloffset value '" + attrs.value("leveloffset") +
                  "'; expected +N, -N or N");
    }
}

int Preprocessor::parseLevelOffset(const QString& raw, bool* ok)
{
    QString s = raw.trimmed();
    if( ok) *ok = false;
    if( s.isEmpty())
        return 0;
    int sign = 1;
    if( s.startsWith('+'))
        s = s.mid(1);
    else if( s.startsWith('-')) {
        sign = -1;
        s = s.mid(1);
    }
    bool numOk = false;
    const int n = s.toInt(&numOk);
    if( !numOk)
        return 0;
    if( ok) *ok = true;
    return sign * n;
}

static void applyLevelOffset(QList<Node*>& nodes, int off)
{
    if( off == 0)
        return;
    for( int i = 0; i < nodes.size(); ++i) {
        Node* n = nodes[i];
        if( !n)
            continue;
        if( n->kind == Node::K_Section) {
            int lv = n->level + off;
            if( lv < 1) lv = 1;
            if( lv > 6) lv = 6;
            n->level = lv;
        }
        applyLevelOffset(n->children, off);
    }
}

bool Preprocessor::resolveInclude(Node* parent, int childIdx, int depth)
{
    Node* inc = parent->children[childIdx];
    if( depth >= dmaxIncludeDepth) {
        error(inc->pos.row, "include:: depth limit exceeded (max " +
              QString::number(dmaxIncludeDepth) + ")");
        return false;
    }

    // parse target: "path[attrs]"
    QString raw = inc->target.trimmed();
    QString path, attrsStr;
    int lb = raw.indexOf('[');
    if( lb >= 0) {
        path = raw.left(lb).trimmed();
        int rb = raw.lastIndexOf(']');
        if( rb > lb)
            attrsStr = raw.mid(lb+1, rb-(lb+1));
    } else
        path = raw;

    // resolve relative path
    QString absPath;
    if( QFileInfo(path).isRelative())
        absPath = dbaseDir + "/" + path;
    else
        absPath = path;

    QString canonical = QFileInfo(absPath).canonicalFilePath();
    if( canonical.isEmpty())
        canonical = absPath;
    if( dincludeStack.contains(canonical)) {
        error(inc->pos.row, "circular include detected: " + path);
        return false;
    }

    QString content = readFile(absPath);
    if( content.isNull()) {
        error(inc->pos.row, "include file not found: " + path);
        // remove the include node, continue
        parent->children.removeAt(childIdx);
        Node::deleteTree(inc);
        return true;
    }

    const QMap<QString,QString> incAttrs = parseIncludeAttrs(attrsStr);
    checkIncludeAttrs(incAttrs, inc->pos.row);

    QStringList lines = content.split('\n');
    if( incAttrs.contains("tag"))
        lines = filterByTag(lines, incAttrs.value("tag"));
    if( incAttrs.contains("tags"))
        lines = filterByTags(lines, incAttrs.value("tags"));
    if( incAttrs.contains("lines"))
        lines = filterByLines(lines, incAttrs.value("lines"));

    content = lines.join('\n');

    // parse included content
    dincludeStack.insert(canonical);
    Parser parser;
    Node* subdoc = parser.parse(content, absPath);
    dincludeStack.remove(canonical);

    if( !subdoc) {
        error(inc->pos.row, "failed to parse included file: " + path);
        parent->children.removeAt(childIdx);
        Node::deleteTree(inc);
        return true;
    }

    for( int i = 0; i < parser.errors.size(); ++i)
        error(inc->pos.row, "[" + path + ":" +
              QString::number(parser.errors[i].pos.row) + "] " +
              parser.errors[i].message);

    // splice included children into parent, replacing the include node
    parent->children.removeAt(childIdx);
    Node::deleteTree(inc);

    QList<Node*> included = subdoc->children;
    subdoc->children.clear(); // detach before deleting subdoc shell

    if( incAttrs.contains("leveloffset")) {
        bool ok = false;
        const int off = parseLevelOffset(incAttrs.value("leveloffset"), &ok);
        if( ok)
            applyLevelOffset(included, off);
    }

    for( int i = 0; i < included.size(); ++i)
        parent->children.insert(childIdx + i, included[i]);

    // recursively process included content
    QString oldBase = dbaseDir;
    dbaseDir = QFileInfo(absPath).path();
    for( int i = 0; i < included.size(); ++i)
        processNode(included[i], depth + 1);
    dbaseDir = oldBase;

    delete subdoc->meta;
    delete subdoc;
    return true;
}

bool Preprocessor::evaluateConditional(Node* parent, int childIdx)
{
    Node* dir = parent->children[childIdx];
    bool isIfdef = (dir->name == "ifdef");

    // parse condition: "attr1,attr2[optional text]"
    QString raw = dir->text.trimmed();
    int lb = raw.indexOf('[');
    QString condStr = (lb >= 0) ? raw.left(lb).trimmed() : raw;

    // check condition: OR logic (any attribute defined)
    QStringList attrs = condStr.split(',', Qt::SkipEmptyParts);
    bool anyDefined = false;
    for( int i = 0; i < attrs.size(); ++i) {
        if( dattrs.contains(attrs[i].trimmed())) {
            anyDefined = true;
            break;
        }
    }

    bool include = isIfdef ? anyDefined : !anyDefined;

    // remove the directive node from parent
    parent->children.removeAt(childIdx);

    if( include) {
        // splice body children in place of the directive
        QList<Node*> body = dir->children;
        dir->children.clear();

        // remove the endif node from the body (last child if present)
        if( !body.isEmpty()) {
            Node* last = body.last();
            if( last && last->kind == Node::K_Directive && last->name == "endif") {
                body.removeLast();
                Node::deleteTree(last);
            }
        }

        for( int i = 0; i < body.size(); ++i)
            parent->children.insert(childIdx + i, body[i]);

        Node::deleteTree(dir);
    } else {
        // discard body
        Node::deleteTree(dir);
    }

    return true;
}

void Preprocessor::expandVerbatimIncludes(Node* n, int depth)
{
    // include:: directives are also expanded inside listing blocks (----)
    // the included content is inserted as-is
    // literal (....) blocks are left untouched (so they can show include:: examples)
    if( !n->text.contains("include::"))
        return;
    if( depth >= dmaxIncludeDepth) {
        error(n->pos.row, "include:: depth limit exceeded (max " +
              QString::number(dmaxIncludeDepth) + ")");
        return;
    }
    const QStringList lines = n->text.split('\n');
    QStringList out;
    for( int i = 0; i < lines.size(); ++i) {
        const QString trimmed = lines[i].trimmed();
        if( trimmed.startsWith("include::") && trimmed.endsWith("]")) {
            const QString body = trimmed.mid(9);
            const int lb = body.indexOf('[');
            if( lb >= 0) {
                const QString incPath = body.left(lb).trimmed();
                const int rb = body.lastIndexOf(']');
                const QString attrsStr = (rb > lb) ? body.mid(lb+1, rb-(lb+1)) : QString();

                QString absPath;
                if( QFileInfo(incPath).isRelative())
                    absPath = dbaseDir + "/" + incPath;
                else
                    absPath = incPath;

                const QString content = readFile(absPath);
                if( content.isNull()) {
                    error(n->pos.row, "include file not found: " + incPath);
                    continue;
                }

                const QMap<QString,QString> incAttrs = parseIncludeAttrs(attrsStr);
                checkIncludeAttrs(incAttrs, n->pos.row);

                QStringList incLines = content.split('\n');
                if( incAttrs.contains("tag"))
                    incLines = filterByTag(incLines, incAttrs.value("tag"));
                if( incAttrs.contains("tags"))
                    incLines = filterByTags(incLines, incAttrs.value("tags"));
                if( incAttrs.contains("lines"))
                    incLines = filterByLines(incLines, incAttrs.value("lines"));

                out += incLines;
                continue;
            }
        }
        out.append(lines[i]);
    }
    n->text = out.join('\n');
}

QString Preprocessor::rewriteImagePaths(const QString& line, const QString& fromDir)
{
    // rewrite relative image: / image:: paths so they resolve from doutDir
    if( doutDir.isEmpty())
        return line;

    static const QRegularExpression re("image:{1,2}([^\\[\\]\\s]+)\\[");
    QString result;
    int last = 0;
    QRegularExpressionMatchIterator it = re.globalMatch(line);
    while( it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        result += line.mid(last, m.capturedStart(1) - last); // keep "image:"/"image::"
        QString path = m.captured(1);
        if( QFileInfo(path).isRelative()) {
            const QString abs = QFileInfo(fromDir + "/" + path).absoluteFilePath();
            path = QDir(doutDir).relativeFilePath(abs);
        }
        result += path;
        last = m.capturedEnd(1);
    }
    result += line.mid(last);
    return result;
}

QString Preprocessor::flatten(const QString& filePath, const QString& outPath, bool compat)
{
    // expand all include:: directives at the source-text level, returning a single self-contained document
    // tag=/lines= filters are applied; other content is preserved verbatim
    errors.clear();
    dincludeStack.clear();
    doutDir = outPath.isEmpty() ? QString() : QFileInfo(outPath).absolutePath();
    const QString abs = QFileInfo(filePath).absoluteFilePath();
    QStringList lines = expandFile(abs, QString(), 0, 0, false);
    if( compat)
        lines = toAsciidoctorCompat(lines);
    return lines.join('\n');
}

static bool isVerbatimFence(const QString& trimmed)
{
    return trimmed == "----" || trimmed == "...." ||
           trimmed == "++++" || trimmed == "////";
}

static bool isListingFence(const QString& trimmed)
{
    return trimmed == "----";
}

static QString shiftHeadingLine(const QString& line, int off)
{
    // shift a section heading line (=..====== followed by space) by off levels
    int marks = 0;
    while( marks < line.size() && line[marks] == '=')
        ++marks;
    if( marks < 1 || marks >= line.size() || line[marks] != ' ')
        return line;
    int newMarks = marks + off;
    if( newMarks < 1) newMarks = 1;
    if( newMarks > 6) newMarks = 6;
    return QString(newMarks, '=') + line.mid(marks);
}

QStringList Preprocessor::toAsciidoctorCompat(const QStringList& lines)
{
    // rewrite the flattened document to the shape Asciidoctor's book doctype expects
    // parts as level-0 headings, the sections inside a part shifted up one level
    // the front matter marked [preface], and the book attributes added to the header

    bool hasParts = false;
    bool numbered = false;
    bool fence = false;
    for( int i = 0; i < lines.size(); ++i) {
        const QString trimmed = lines[i].trimmed();
        if( isVerbatimFence(trimmed)) {
            fence = !fence;
            continue;
        }
        if( fence)
            continue;
        if( trimmed == "[part]")
            hasParts = true;
        else if( trimmed == ":numbered:" || trimmed == ":sectnums:")
            numbered = true;
    }

    QStringList out;
    QString curFence;
    bool seenTitle = false;
    bool inHeader = false;   // between the document title and the first body line
    bool inBody = false;     // from the first part opener on
    bool partPending = false;

    for( int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        const QString trimmed = line.trimmed();

        if( isVerbatimFence(trimmed)) {
            if( curFence.isEmpty())
                curFence = trimmed;
            else if( curFence == trimmed)
                curFence.clear();
            out.append(line);
            continue;
        }
        if( !curFence.isEmpty()) {
            out.append(line);
            continue;
        }

        if( inHeader) {
            if( trimmed.startsWith(':')) {
                // ':numbered:'/':sectnums:'/':doctype:' are superseded by the
                // compat attributes; everything else is kept
                if( trimmed.startsWith(":numbered:") ||
                        trimmed.startsWith(":sectnums:") ||
                        trimmed.startsWith(":doctype:")) {
                    continue;
                }
                out.append(line);
                continue;
            }
            // header ends at the first non-attribute line
            out.append(":doctype: book");
            if( numbered) {
                out.append(":sectnums:");
                out.append(":sectnumlevels: 3");
            }
            if( hasParts) {
                out.append(":partnums:");
                out.append(":part-signifier: Part");
            }
            out.append(":chapter-signifier: Chapter");
            out.append(":appendix-caption: Appendix");
            inHeader = false;
            // fall through to normal handling of this line
        }

        if( hasParts && trimmed == "[part]") {
            // Asciidoctor uses a level-0 heading instead of a part role
            partPending = true;
            continue;
        }

        int marks = 0;
        while( marks < line.size() && line[marks] == QChar('='))
            ++marks;
        const bool isHeading = marks >= 1 && marks < line.size() &&
                line[marks] == QChar(' ');
        if( isHeading) {
            if( marks == 1 && !seenTitle) {
                seenTitle = true;
                inHeader = true;
                out.append(line);
                continue;
            }
            if( marks == 2 && partPending) {
                partPending = false;
                inBody = true;
                out.append("=" + line.mid(2));
                continue;
            }
            if( hasParts && !inBody) {
                // front matter: the preface keeps its level and is unnumbered
                if( marks == 2)
                    out.append("[preface]");
                out.append(line);
                continue;
            }
            if( inBody && marks >= 3) {
                // sections inside a part move up one level
                out.append(line.mid(1));
                continue;
            }
        }
        out.append(line);
    }
    return out;
}

QStringList Preprocessor::expandFile(const QString& absPath, const QString& attrsStr, int depth, int levelShift, bool verbatim)
{
    QStringList out;
    if( depth >= dmaxIncludeDepth) {
        error(0, "include:: depth limit exceeded (max " +
              QString::number(dmaxIncludeDepth) + ")");
        return out;
    }

    QString canonical = QFileInfo(absPath).canonicalFilePath();
    if( canonical.isEmpty())
        canonical = absPath;
    if( dincludeStack.contains(canonical)) {
        error(0, "circular include detected: " + absPath);
        return out;
    }

    const QString content = readFile(absPath);
    if( content.isNull()) {
        error(0, "include file not found: " + absPath);
        return out;
    }

    // apply include attribute filters to the raw file content
    const QMap<QString,QString> incAttrs = parseIncludeAttrs(attrsStr);
    checkIncludeAttrs(incAttrs, 0);

    QStringList lines = content.split('\n');
    if( incAttrs.contains("tag"))
        lines = filterByTag(lines, incAttrs.value("tag"));
    if( incAttrs.contains("tags"))
        lines = filterByTags(lines, incAttrs.value("tags"));
    if( incAttrs.contains("lines"))
        lines = filterByLines(lines, incAttrs.value("lines"));

    if( incAttrs.contains("leveloffset")) {
        bool ok = false;
        const int off = parseLevelOffset(incAttrs.value("leveloffset"), &ok);
        if( ok)
            levelShift += off;
    }

    dincludeStack.insert(canonical);
    const QString baseDir = QFileInfo(absPath).path();

    QString fence; // the fence delimiter we are currently inside, if any
    bool inVerbatim = false;
    for( int i = 0; i < lines.size(); ++i) {
        const QString trimmed = lines[i].trimmed();
        if( !verbatim && isVerbatimFence(trimmed)) {
            if( fence.isEmpty())
                fence = trimmed;
            else if( fence == trimmed)
                fence.clear();
            inVerbatim = !fence.isEmpty();
        }
        const bool expandHere = !verbatim &&
                (fence.isEmpty() || isListingFence(fence));
        if( expandHere && trimmed.startsWith("include::") && trimmed.endsWith("]")) {
            const QString body = trimmed.mid(9); // strip "include::"
            const int lb = body.indexOf('[');
            if( lb >= 0) {
                const QString incPath = body.left(lb).trimmed();
                const int rb = body.lastIndexOf(']');
                const QString incAttrs = (rb > lb) ? body.mid(lb+1, rb-(lb+1)) : QString();

                QString incAbs;
                if( QFileInfo(incPath).isRelative())
                    incAbs = baseDir + "/" + incPath;
                else
                    incAbs = incPath;
                incAbs = QFileInfo(incAbs).absoluteFilePath();

                out += expandFile(incAbs, incAttrs, depth + 1, levelShift,
                                  verbatim || inVerbatim);
                continue;
            }
        }
        QString outLine = lines[i];
        if( verbatim || inVerbatim) {
            out.append(outLine);
            continue;
        }
        if( levelShift != 0)
            outLine = shiftHeadingLine(outLine, levelShift);
        out.append(rewriteImagePaths(outLine, baseDir));
    }

    dincludeStack.remove(canonical);
    return out;
}

QString Preprocessor::readFile(const QString& path)
{
    QFile f(path);
    if( !f.open(QIODevice::ReadOnly))
        return QString(); // null string indicates error
    QByteArray bytes = f.readAll();
    return QString::fromUtf8(bytes.constData(), bytes.size());
}

QStringList Preprocessor::filterByTag(const QStringList& lines, const QString& tag)
{
    // look for "// tag::name[]" and "// end::name[]" markers
    QStringList result;
    bool inTag = false;
    const QString startMarker = "tag::" + tag + "[]";
    const QString endMarker = "end::" + tag + "[]";

    for( int i = 0; i < lines.size(); ++i) {
        const QString trimmed = lines[i].trimmed();
        if( trimmed.contains(startMarker)) {
            inTag = true;
            continue;
        }
        if( trimmed.contains(endMarker)) {
            inTag = false;
            continue;
        }
        if( inTag)
            result.append(lines[i]);
    }
    return result;
}

QStringList Preprocessor::filterByTags(const QStringList& lines, const QString& tagsSpec)
{
    // include the union of several tagged regions: tags=name1;name2
    QStringList result;
    const QStringList tags = tagsSpec.split(';', Qt::SkipEmptyParts);
    QSet<QString> active;

    for( int i = 0; i < lines.size(); ++i) {
        const QString trimmed = lines[i].trimmed();
        bool isMarker = false;
        for( int t = 0; t < tags.size(); ++t) {
            const QString tag = tags[t].trimmed();
            if( trimmed.contains("tag::" + tag + "[]")) {
                active.insert(tag);
                isMarker = true;
            } else if( trimmed.contains("end::" + tag + "[]")) {
                active.remove(tag);
                isMarker = true;
            }
        }
        if( isMarker)
            continue;
        if( !active.isEmpty())
            result.append(lines[i]);
    }
    return result;
}

QStringList Preprocessor::filterByLines(const QStringList& lines, const QString& spec)
{
    // parse "N..M" or "N;M;P" or "N..M;P..Q"
    QStringList result;
    QStringList ranges = spec.split(';', Qt::SkipEmptyParts);

    for( int r = 0; r < ranges.size(); ++r) {
        QString range = ranges[r].trimmed();
        int dots = range.indexOf("..");
        if( dots >= 0) {
            int start = range.left(dots).toInt();
            int end = range.mid(dots+2).toInt();
            if( start < 1)
                start = 1;
            if( end < 0 || end > lines.size())
                end = lines.size();
            for( int i = start-1; i < end && i < lines.size(); ++i)
                result.append(lines[i]);
        } else {
            int line = range.toInt();
            if( line >= 1 && line <= lines.size())
                result.append(lines[line-1]);
        }
    }
    return result;
}
