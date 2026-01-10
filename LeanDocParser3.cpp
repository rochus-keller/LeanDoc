/*
 * LeanDocParser2.cpp
 * Fixed recursive descent parser for LeanDoc/AsciiDoc
 *
 * Fixes:
 * - Metadata ([[id]], [role]) before a section title no longer gets swallowed
 *   by the preceding section's body.
 * - Optimized table cell splitting (handles escaped pipes).
 * - Compatible with Qt5 / C++98.
 */

#include <QString>
#include <QStringList>
#include <QList>
#include <QRegExp>
#include <QDebug>
#include <QStack>

// --- AST Node Stubs (Implicit based on your output) ---
enum NodeType { Node_Section, Node_Paragraph, Node_Table, Node_List, Node_ThematicBreak };

class Node {
public:
    virtual ~Node() { qDeleteAll(children); }
    NodeType type;
    QList<Node*> children;
    QString id;
    QString role;
    // ... other metadata fields
};

class Section : public Node {
public:
    Section() { type = Node_Section; level = 0; }
    int level;
    QString title;
};

class Block : public Node {
public:
    Block(NodeType t) { type = t; }
    QString content; // simplified
};

class Table : public Node {
public:
    Table() { type = Node_Table; }
};

// --- Parser Implementation ---

class LeanDocParser {
public:
    LeanDocParser(const QStringList& lines) : m_lines(lines), m_pos(0) {}

    Node* parse() {
        Section* root = new Section();
        root->level = 0; // Document root
        root->title = "Document";
        
        parseSectionBody(root);
        return root;
    }

private:
    QStringList m_lines;
    int m_pos;

    // --- Helper: Peek at line relative to current position ---
    QString peekLine(int offset = 0) const {
        if (m_pos + offset >= m_lines.size()) return QString();
        return m_lines[m_pos + offset];
    }

    // --- Helper: Check for Section Header (== Title) ---
    // Returns level (1-6) or 0 if not a header
    int getSectionLevel(const QString& line) const {
        if (line.isEmpty()) return 0;
        // Simple regex: ^={1,6}\s+
        int level = 0;
        while (level < line.length() && line[level] == '=') {
            level++;
        }
        if (level > 0 && level <= 6 && level < line.length() && line[level] == ' ') {
            return level;
        }
        return 0; // Not a section header
    }

    // --- Helper: Metadata Collector ---
    struct BlockMetadata {
        QString id;
        QString role;
        QStringList attributes;
        int lineCount; // How many lines of metadata were found
        
        bool isEmpty() const { return lineCount == 0; }
    };

    // Scans ahead for metadata but DOES NOT consume lines yet.
    // Returns what it found and how many lines it occupies.
    BlockMetadata peekMetadata() const {
        BlockMetadata meta;
        meta.lineCount = 0;
        
        int offset = 0;
        while (m_pos + offset < m_lines.size()) {
            QString line = m_lines[m_pos + offset].trimmed();
            if (line.isEmpty()) {
                // Skip blank lines before metadata? 
                // Usually metadata must be attached directly, but we might skip one blank.
                // For strict AsciiDoc, metadata is contiguous above the block.
                // We'll stop if we hit a blank line *after* finding some metadata?
                // Simplified: blank lines break the metadata chain unless it's strictly above.
                // Let's assume metadata lines are contiguous.
                if (meta.lineCount == 0) { offset++; continue; } // Skip leading blanks
                else break; // Blank line ends metadata block
            }

            if (line.startsWith("[[") && line.endsWith("]]")) {
                // Anchor: [[id]]
                meta.id = line.mid(2, line.length() - 4);
                offset++;
                meta.lineCount++;
            } 
            else if (line.startsWith("[") && line.endsWith("]")) {
                // Attribute list: [bibliography]
                meta.attributes << line.mid(1, line.length() - 2);
                offset++;
                meta.lineCount++;
            } 
            else if (line.startsWith(".") && !line.startsWith("..")) {
                // Role or Title: .Title or .role
                // Simplified detection
                offset++;
                meta.lineCount++;
            } 
            else {
                // Not a metadata line -> done peeking
                break;
            }
        }
        // meta.lineCount now equals the number of lines (including skipped blanks if logic allowed)
        // that constitute the metadata block.
        // NOTE: If we skipped leading blanks, actual lines consumed = offset.
        meta.lineCount = offset; 
        return meta;
    }

    // --- Core Recursive Logic ---

    void parseSectionBody(Section* currentSection) {
        while (m_pos < m_lines.size()) {
            
            // 1. Peek at potential metadata (attributes, anchors, etc.)
            //    We do NOT consume it yet.
            BlockMetadata meta = peekMetadata();
            
            // 2. Identify the "Target Block" (the line after the metadata)
            int targetLineIndex = m_pos + meta.lineCount;
            QString targetLine = (targetLineIndex < m_lines.size()) ? m_lines[targetLineIndex] : QString();
            
            int nextSectionLevel = getSectionLevel(targetLine);

            // --- THE FIX IS HERE ---
            if (nextSectionLevel > 0) {
                // We found a section title. Check nesting.
                if (nextSectionLevel <= currentSection->level) {
                    // It is a sibling (same level) or ancestor (lower level).
                    // We must STOP parsing this section.
                    // IMPORTANT: We do NOT consume the metadata. It belongs to the *next* section.
                    return; 
                } else {
                    // It is a CHILD section (nested).
                    // We consume the metadata, create the child, and recurse.
                    m_pos += meta.lineCount; // Consume metadata
                    m_pos++;                 // Consume title line
                    
                    Section* subSection = new Section();
                    subSection->level = nextSectionLevel;
                    subSection->title = targetLine.mid(nextSectionLevel).trimmed();
                    subSection->id = meta.id; // Apply metadata
                    // subSection->attributes = meta.attributes; 
                    
                    currentSection->children.append(subSection);
                    
                    // Recurse into the child
                    parseSectionBody(subSection); 
                    continue; 
                }
            }
            // -----------------------

            // If we are here, the next content is NOT a section title (or it was a child section handled above).
            // It must be a regular block (Paragraph, Table, List, etc.)
            
            // Now we can safely consume the metadata for this block.
            m_pos += meta.lineCount;
            
            // Check for EOF after consuming metadata
            if (m_pos >= m_lines.size()) break;

            QString line = m_lines[m_pos];
            
            // -- Detect Block Type --
            
            if (line == "'''") {
                // Thematic Break
                m_pos++;
                currentSection->children.append(new Block(Node_ThematicBreak));
            }
            else if (line.startsWith("|===")) {
                // Table
                parseTable(currentSection, meta);
            }
            else if (line.trimmed().isEmpty()) {
                // Skip blank lines inside body
                m_pos++; 
            }
            else {
                // Default: Paragraph
                Block* para = new Block(Node_Paragraph);
                para->id = meta.id;
                // Read paragraph lines until blank line
                while (m_pos < m_lines.size() && !m_lines[m_pos].trimmed().isEmpty() && getSectionLevel(m_lines[m_pos]) == 0) {
                     para->content += m_lines[m_pos] + "\n";
                     m_pos++;
                }
                currentSection->children.append(para);
            }
        }
    }

    void parseTable(Section* parent, const BlockMetadata& meta) {
        Table* table = new Table();
        table->id = meta.id;
        parent->children.append(table);
        
        m_pos++; // Skip start delimiter |===
        
        while (m_pos < m_lines.size()) {
            QString line = m_lines[m_pos];
            if (line.startsWith("|===")) {
                m_pos++; // End delimiter
                break;
            }
            
            if (line.startsWith("|")) {
                // Using the optimized splitter from previous conversation
                QStringList cells = splitOnUnescapedPipe(line.mid(1)); // skip first '|'
                // ... create Row/Cells from 'cells' ...
            }
            m_pos++;
        }
    }

    // Optimized splitter for table cells (handles \| escaping)
    QStringList splitOnUnescapedPipe(const QString& s) {
        QStringList r; QString cur; int bs = 0;
        for (int i = 0; i < s.size(); ++i) {
            const QChar c = s[i];
            if (c == '|') {
                if (bs & 1) { cur.chop(1); cur += '|'; } // Unescape \|
                else { r << cur; cur.clear(); }
                bs = 0;
            } else {
                cur += c;
                bs = (c == '\\') ? (bs + 1) : 0;
            }
        }
        r << cur;
        return r;
    }
};

