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

    // apply tag filter
    QStringList lines = content.split('\n');
    if( attrsStr.contains("tag=")) {
        int ti = attrsStr.indexOf("tag=");
        int te = attrsStr.indexOf(',', ti);
        QString tag = (te > ti) ? attrsStr.mid(ti+4, te-(ti+4)).trimmed()
                                : attrsStr.mid(ti+4).trimmed();
        lines = filterByTag(lines, tag);
    }

    // apply lines filter
    if( attrsStr.contains("lines=")) {
        int li = attrsStr.indexOf("lines=");
        int le = attrsStr.indexOf(',', li);
        QString spec = (le > li) ? attrsStr.mid(li+6, le-(li+6)).trimmed()
                                 : attrsStr.mid(li+6).trimmed();
        lines = filterByLines(lines, spec);
    }

    content = lines.join('\n');

    // parse included content
    dincludeStack.insert(canonical);
    Parser parser;
    Node* subdoc = parser.parse(content);
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
