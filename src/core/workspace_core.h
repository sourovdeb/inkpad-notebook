#ifndef WORKSPACE_CORE_H
#define WORKSPACE_CORE_H
#include <algorithm>
#include <string>
#include <vector>
#include <cstddef>
namespace Workspace {
constexpr size_t MAX_DOCUMENT = 512 * 1024;
bool validUtf8(const std::string &s);
size_t previous(const std::string &s, size_t p);
size_t next(const std::string &s, size_t p);
bool validName(const std::string &name);
bool readDocument(const std::string &path, std::string &out, std::string &error);
bool saveDocument(const std::string &path, const std::string &text, std::string &error);
struct Snapshot { std::string text; size_t cursor, anchor; };
class Editor {
public:
    std::string text;
    size_t cursor = 0, anchor = 0;
    std::vector<Snapshot> history, future;
    void load(const std::string &s);
    size_t low() const { return std::min(cursor, anchor); }
    size_t high() const { return std::max(cursor, anchor); }
    std::string selected() const { return text.substr(low(), high()-low()); }
    void move(size_t p, bool extend = false);
    bool insert(const std::string &s);
    bool erase(bool backwards);
    bool undo();
    bool redo();
    bool wrap(const std::string &marker);
    bool heading(int level);
    bool find(const std::string &term);
private:
    void remember();
};
struct Span { std::string text; int style; }; // bit 1 bold, bit 2 italic
struct Block { int heading; std::vector<Span> spans; };
std::vector<Block> parseMarkdown(const std::string &text);
std::string html(const std::string &text, const std::string &title, bool markdown);
std::string plain(const std::string &text, bool markdown);
}
#endif
