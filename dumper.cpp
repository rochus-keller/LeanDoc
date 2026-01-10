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

#include <QtCore/QCoreApplication>
#include <QtCore/QStringList>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtDebug>

#include "LeanDocLexer2.h"
#include "LeanDocParser2.h"
#include "LeanDocAst2.h"

using namespace LeanDoc;


static void dumpTokens(const QString& input, QTextStream& out)
{
    Lexer lex;
    lex.setInput(input);

    while (!lex.atEnd()) {
        LineTok t = lex.take();
        out << t.lineNo << ": " << t.kindName();
        if (t.level)
            out << " level=" << t.level;
        if (!t.head.isEmpty())
            out << " head=\"" << t.head << "\"";
        if (!t.rest.isEmpty())
            out << " rest=\"" << t.rest << "\"";
        out << "\n";
    }
    // EOF token
    LineTok eof = lex.take();
    out << eof.lineNo << ": " << eof.kindName() << "\n";
}

static void indent(QTextStream& out, int n)
{
    for (int i=0;i<n;i++)
        out << "  ";
}


static void dumpMeta(const BlockMeta* m, QTextStream& out)
{
    if (!m) return;
    if (!m->anchorId.isEmpty())
        out << " anchorId=\"" << m->anchorId << "\"";
    if (!m->anchorText.isEmpty())
        out << " anchorText=\"" << m->anchorText << "\"";
    if (!m->title.isEmpty())
        out << " title=\"" << m->title << "\"";
    if (!m->attrs.isEmpty())
        out << " attrs=" << m->attrs.size();
}

static void dumpNode(const Node* n, QTextStream& out, int depth)
{
    if (!n)
        return;
    indent(out, depth);
    out << n->nodeKindName() << " @" << n->pos.line;

    dumpMeta(n->meta, out);

    if (!n->name.isEmpty())
        out << " name=\"" << n->name << "\"";
    if (!n->target.isEmpty())
        out << " target=\"" << n->target << "\"";
    if (!n->text.isEmpty())
    {
        QString str = n->text;
        if( str.length() > 64 )
        {
            str = str.simplified().left(64);
            str = "\"" + str + "\"";
            str += "...";
        }else
            str = "\"" + str + "\"";
        out << " text=" << str;
    }
    if (!n->kv.isEmpty())
        out << " kv=" << n->kv.size();

    out << "\n";

    for (int i=0;i<n->children.size();++i) {
        dumpNode(n->children[i], out, depth+1);
    }
}

static bool readFileUtf8(const QString& path, QString* outText, QString* outErr)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (outErr) *outErr = "Cannot open file: " + path;
        return false;
    }
    QByteArray bytes = f.readAll();
    *outText = QString::fromUtf8(bytes.constData(), bytes.size());
    return true;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    QTextStream out(stdout);
    QTextStream err(stderr);

    const QStringList args = app.arguments();
    if (args.size() < 2) {
        err << "Usage:\n"
            << "  dumper --tokens <file>\n"
            << "  dumper --ast    <file>\n";
        return 2;
    }

    bool modeTokens = false;
    bool modeAst = false;
    QString filePath;

    for (int i=1;i<args.size();++i) {
        if (args[i] == "--tokens")
            modeTokens = true;
        else if (args[i] == "--ast")
            modeAst = true;
        else
            filePath = args[i];
    }

    if (filePath.isEmpty() || (modeTokens == modeAst)) {
        err << "Error: specify exactly one mode and a file.\n";
        return 2;
    }

    QString text, ioErr;
    if (!readFileUtf8(filePath, &text, &ioErr)) {
        err << ioErr << "\n";
        return 2;
    }

    if (modeTokens) {
        dumpTokens(text, out);
        return 0;
    }

    // modeAst
    Parser p;
    ParseError pe;
    Node* doc = p.parse(text, &pe);
    if (!doc) {
        err << "Parse error at line " << pe.line << ": " << pe.message << "\n";
        return 1;
    }

    dumpNode(doc, out, 0);
    Node::deleteTree(doc);
    return 0;
}

