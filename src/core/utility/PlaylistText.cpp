/*
 * PlaylistText.cpp - read M3U, PLS, XSPF and ASX playlists
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace Core {
namespace Utility {

namespace {

/**
 * @brief Lower-case ASCII letters only, so no locale is involved.
 * @param s The text.
 * @return It, lower-cased.
 */
std::string plLower(std::string s)
{
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

/**
 * @brief Strip surrounding whitespace and line endings.
 * @param s The text.
 * @return It, trimmed.
 */
std::string plTrim(const std::string& s)
{
    const size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return std::string();
    const size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

/**
 * @brief The text without a leading UTF-8 byte order mark.
 * @param text The text.
 * @return It, without the mark.
 */
std::string plNoBom(const std::string& text)
{
    return text.compare(0, 3, "\xEF\xBB\xBF") == 0 ? text.substr(3) : text;
}

/**
 * @brief An element's name without a namespace prefix, lower-cased.
 * @param node The element.
 * @return Its local name.
 */
std::string plLocalName(const pugi::xml_node& node)
{
    std::string name = node.name();
    const size_t colon = name.find(':');
    if (colon != std::string::npos) name.erase(0, colon + 1);
    return plLower(name);
}

/**
 * @brief The trimmed text of the first child element called @p name (any
 *        case, any namespace prefix).
 * @param parent Where to look.
 * @param name Lower-case local name.
 * @return Its text, or "".
 */
std::string plChildText(const pugi::xml_node& parent, const char* name)
{
    for (pugi::xml_node child = parent.first_child(); child; child = child.next_sibling()) {
        if (child.type() == pugi::node_element && plLocalName(child) == name) {
            return plTrim(child.text().get());
        }
    }
    return std::string();
}

/**
 * @brief An attribute's value, its name matched in any case.
 * @param node The element.
 * @param name Lower-case attribute name.
 * @return The value, or "".
 */
std::string plAttribute(const pugi::xml_node& node, const char* name)
{
    for (pugi::xml_attribute a = node.first_attribute(); a; a = a.next_attribute()) {
        if (plLower(a.name()) == name) return plTrim(a.value());
    }
    return std::string();
}

/**
 * @brief Visit every element under @p node (and @p node itself), depth first.
 * @param node Where to start.
 * @param visit Called for each element.
 */
void plWalk(const pugi::xml_node& node, const std::function<void(const pugi::xml_node&)>& visit)
{
    if (node.type() == pugi::node_element) visit(node);
    for (pugi::xml_node child = node.first_child(); child; child = child.next_sibling()) {
        plWalk(child, visit);
    }
}

} // namespace

/**
 * @brief Map a file extension to a format.
 * @param extension Without the dot.
 * @return The format.
 */
PlaylistFormat PlaylistText::formatForExtension(const std::string& extension)
{
    const std::string e = plLower(extension);
    if (e == "m3u" || e == "m3u8") return PlaylistFormat::M3U;
    if (e == "pls") return PlaylistFormat::PLS;
    if (e == "xspf") return PlaylistFormat::XSPF;
    if (e == "asx" || e == "wax" || e == "wvx") return PlaylistFormat::ASX;
    return PlaylistFormat::Unknown;
}

/**
 * @brief Map a media type to a format.
 * @param mime_type Lower-case type.
 * @return The format.
 */
PlaylistFormat PlaylistText::formatForMimeType(const std::string& mime_type)
{
    if (mime_type == "audio/x-mpegurl" || mime_type == "audio/mpegurl" ||
        mime_type == "application/x-mpegurl" || mime_type == "application/vnd.apple.mpegurl") {
        return PlaylistFormat::M3U;
    }
    if (mime_type == "audio/x-scpls" || mime_type == "audio/scpls") return PlaylistFormat::PLS;
    if (mime_type == "application/xspf+xml") return PlaylistFormat::XSPF;
    if (mime_type == "video/x-ms-asf" || mime_type == "video/x-ms-asx" ||
        mime_type == "audio/x-ms-wax" || mime_type == "video/x-ms-wvx") {
        return PlaylistFormat::ASX;
    }
    return PlaylistFormat::Unknown;
}

/**
 * @brief Recognise a format from the start of the text.
 * @param text The playlist.
 * @param fallback What the name or type said.
 * @return The format.
 */
PlaylistFormat PlaylistText::detect(const std::string& text, PlaylistFormat fallback)
{
    const std::string head = plLower(plTrim(plNoBom(text)).substr(0, 512));
    if (head.compare(0, 7, "#extm3u") == 0) return PlaylistFormat::M3U;
    if (head.compare(0, 10, "[playlist]") == 0) return PlaylistFormat::PLS;
    /*
     * XML: by its root element, after any declaration, comments and
     * DOCTYPE -- not by tag names found anywhere, which an M3U title or path
     * can hold.
     */
    size_t p = 0;
    for (;;) {
        p = head.find_first_not_of(" \t\r\n", p);
        if (p == std::string::npos) return fallback;
        size_t end;
        if (head.compare(p, 2, "<?") == 0) {
            end = head.find("?>", p);
            if (end != std::string::npos) end += 2;
        } else if (head.compare(p, 4, "<!--") == 0) {
            end = head.find("-->", p);
            if (end != std::string::npos) end += 3;
        } else if (head.compare(p, 2, "<!") == 0) {
            end = head.find('>', p);
            if (end != std::string::npos) end += 1;
        } else {
            break;
        }
        if (end == std::string::npos) return fallback;
        p = end;
    }
    if (head.compare(p, 4, "<asx") == 0) return PlaylistFormat::ASX;
    if (head.compare(p, 9, "<playlist") == 0 &&
        (head.find("xspf.org/ns/0") != std::string::npos || head.find("<tracklist") != std::string::npos)) {
        return PlaylistFormat::XSPF;
    }
    return fallback;
}

/**
 * @brief Parse in the given format.
 * @param text The playlist.
 * @param format Its format.
 * @return The entries.
 */
std::vector<PlaylistItem> PlaylistText::parse(const std::string& text, PlaylistFormat format)
{
    switch (format) {
        case PlaylistFormat::PLS: return parsePLS(text);
        case PlaylistFormat::XSPF: return parseXSPF(text);
        case PlaylistFormat::ASX: return parseASX(text);
        case PlaylistFormat::M3U:
        case PlaylistFormat::Unknown:
        default: return parseM3U(text);
    }
}

/**
 * @brief Read M3U lines.
 * @param text The playlist.
 * @return The entries.
 */
std::vector<PlaylistItem> PlaylistText::parseM3U(const std::string& text)
{
    std::vector<PlaylistItem> items;
    PlaylistItem next;
    const std::string body = plNoBom(text);
    size_t pos = 0;
    while (pos < body.size()) {
        size_t nl = body.find('\n', pos);
        if (nl == std::string::npos) nl = body.size();
        const std::string line = plTrim(body.substr(pos, nl - pos));
        pos = nl + 1;
        if (line.empty()) continue;
        if (line.compare(0, 8, "#EXTINF:") == 0) {
            // #EXTINF:seconds,Artist - Title
            const size_t comma = line.find(',');
            if (comma == std::string::npos) continue;
            next.duration_seconds = std::strtol(line.c_str() + 8, nullptr, 10);
            if (next.duration_seconds < 0) next.duration_seconds = 0;
            const std::string name = line.substr(comma + 1);
            const size_t dash = name.find(" - ");
            if (dash == std::string::npos) {
                next.artist.clear();
                next.title = plTrim(name);
            } else {
                next.artist = plTrim(name.substr(0, dash));
                next.title = plTrim(name.substr(dash + 3));
            }
            continue;
        }
        if (line[0] == '#') continue;
        next.location = line;
        items.push_back(next);
        next = PlaylistItem();
    }
    return items;
}

/**
 * @brief Read PLS keys, numbered entries in order.
 * @param text The playlist.
 * @return The entries.
 */
std::vector<PlaylistItem> PlaylistText::parsePLS(const std::string& text)
{
    std::map<long, PlaylistItem> numbered;
    const std::string body = plNoBom(text);
    size_t pos = 0;
    while (pos < body.size()) {
        size_t nl = body.find('\n', pos);
        if (nl == std::string::npos) nl = body.size();
        const std::string line = plTrim(body.substr(pos, nl - pos));
        pos = nl + 1;
        const size_t eq = line.find('=');
        if (eq == std::string::npos || line.empty() || line[0] == '[' || line[0] == ';') continue;
        const std::string key = plLower(plTrim(line.substr(0, eq)));
        const std::string value = plTrim(line.substr(eq + 1));
        size_t digits = key.size();
        while (digits > 0 && std::isdigit(static_cast<unsigned char>(key[digits - 1]))) --digits;
        if (digits == key.size()) continue; // not numbered: NumberOfEntries, Version
        const long n = std::strtol(key.c_str() + digits, nullptr, 10);
        const std::string field = key.substr(0, digits);
        PlaylistItem& item = numbered[n];
        if (field == "file") {
            item.location = value;
        } else if (field == "title") {
            const size_t dash = value.find(" - ");
            if (dash == std::string::npos) {
                item.title = value;
            } else {
                item.artist = plTrim(value.substr(0, dash));
                item.title = plTrim(value.substr(dash + 3));
            }
        } else if (field == "length") {
            item.duration_seconds = std::max(0L, std::strtol(value.c_str(), nullptr, 10));
        }
    }
    std::vector<PlaylistItem> items;
    for (auto& entry : numbered) {
        if (!entry.second.location.empty()) items.push_back(std::move(entry.second));
    }
    return items;
}

/**
 * @brief Read XSPF tracks.
 * @param text The playlist.
 * @return The entries.
 */
std::vector<PlaylistItem> PlaylistText::parseXSPF(const std::string& text)
{
    std::vector<PlaylistItem> items;
    pugi::xml_document doc;
    if (!doc.load_buffer(text.data(), text.size())) return items;
    plWalk(doc, [&items](const pugi::xml_node& node) {
        if (plLocalName(node) != "track") return;
        PlaylistItem item;
        item.location = plChildText(node, "location");
        if (item.location.empty()) return;
        item.title = plChildText(node, "title");
        item.artist = plChildText(node, "creator");
        const std::string ms = plChildText(node, "duration");
        if (!ms.empty()) item.duration_seconds = std::max(0L, std::strtol(ms.c_str(), nullptr, 10) / 1000);
        items.push_back(std::move(item));
    });
    return items;
}

/**
 * @brief Read ASX entries.
 * @param text The playlist.
 * @return The entries.
 */
std::vector<PlaylistItem> PlaylistText::parseASX(const std::string& text)
{
    std::vector<PlaylistItem> items;
    pugi::xml_document doc;
    if (!doc.load_buffer(text.data(), text.size())) {
        // Old ASX is often not XML at all (href=http://... unquoted).
        return parseLooseASX(text);
    }
    plWalk(doc, [&items](const pugi::xml_node& node) {
        const std::string name = plLocalName(node);
        if (name == "entryref") {
            PlaylistItem item;
            item.location = plAttribute(node, "href");
            if (!item.location.empty()) items.push_back(std::move(item));
            return;
        }
        if (name != "entry") return;
        /*
         * An entry's refs are alternatives, often mms:// first with an http
         * fallback: take the first one that can be opened here (http(s), or
         * a path), else the first there is.
         */
        PlaylistItem item;
        for (pugi::xml_node child = node.first_child(); child; child = child.next_sibling()) {
            if (child.type() != pugi::node_element || plLocalName(child) != "ref") continue;
            const std::string href = plAttribute(child, "href");
            if (href.empty()) continue;
            if (item.location.empty()) item.location = href;
            const std::string scheme = plLower(href.substr(0, 8));
            if (!isUrl(href) || scheme.compare(0, 7, "http://") == 0 || scheme.compare(0, 8, "https://") == 0) {
                item.location = href;
                break;
            }
        }
        if (item.location.empty()) return;
        item.title = plChildText(node, "title");
        item.artist = plChildText(node, "author");
        items.push_back(std::move(item));
    });
    return items;
}

/**
 * @brief Read ASX that is not well-formed XML, tag by tag: each entry's
 *        first openable ref, each entryref; href values quoted or not.
 * @param text The playlist.
 * @return Its entries (no titles).
 */
std::vector<PlaylistItem> PlaylistText::parseLooseASX(const std::string& text)
{
    std::vector<PlaylistItem> items;
    const std::string lower = plLower(text);
    /** @brief The href value in the tag starting at @p tag, or "". */
    auto hrefIn = [&](size_t tag) {
        const size_t close = lower.find('>', tag);
        const size_t at = lower.find("href", tag);
        if (at == std::string::npos || (close != std::string::npos && at > close)) return std::string();
        size_t p = lower.find_first_not_of(" \t\r\n", at + 4);
        if (p == std::string::npos || text[p] != '=') return std::string();
        p = text.find_first_not_of(" \t\r\n", p + 1);
        if (p == std::string::npos) return std::string();
        if (text[p] == '"' || text[p] == '\'') {
            const size_t end = text.find(text[p], p + 1);
            return end == std::string::npos ? std::string() : plTrim(text.substr(p + 1, end - p - 1));
        }
        size_t end = text.find_first_of(" \t\r\n>", p);
        if (end == std::string::npos) end = text.size();
        // "href=http://h/s />": the slash before ">" closes the tag.
        if (end > p && text[end - 1] == '/' && (end == text.size() || text[end] != '/')) --end;
        return text.substr(p, end - p);
    };
    /** @brief Whether this player can open @p href itself: http(s) or a path. */
    auto openable = [](const std::string& href) {
        const std::string scheme = plLower(href.substr(0, 8));
        return !isUrl(href) || scheme.compare(0, 7, "http://") == 0 || scheme.compare(0, 8, "https://") == 0;
    };
    PlaylistItem entry;
    bool in_entry = false;
    bool settled = false;
    for (size_t pos = lower.find('<'); pos != std::string::npos; pos = lower.find('<', pos + 1)) {
        if (lower.compare(pos, 9, "<entryref") == 0) {
            PlaylistItem ref;
            ref.location = hrefIn(pos);
            if (!ref.location.empty()) items.push_back(std::move(ref));
        } else if (lower.compare(pos, 6, "<entry") == 0) {
            entry = PlaylistItem();
            in_entry = true;
            settled = false;
        } else if (lower.compare(pos, 7, "</entry") == 0) {
            if (in_entry && !entry.location.empty()) items.push_back(entry);
            in_entry = false;
        } else if (in_entry && !settled && lower.compare(pos, 4, "<ref") == 0) {
            const std::string href = hrefIn(pos);
            if (href.empty()) continue;
            if (entry.location.empty()) entry.location = href;
            if (openable(href)) {
                entry.location = href;
                settled = true;
            }
        }
    }
    return items;
}

/**
 * @brief Whether a location starts with scheme://.
 * @param location As written.
 * @return true for a URL.
 */
bool PlaylistText::isUrl(const std::string& location)
{
    const size_t sep = location.find("://");
    if (sep == std::string::npos || sep == 0) return false;
    for (size_t i = 0; i < sep; ++i) {
        const unsigned char c = static_cast<unsigned char>(location[i]);
        if (!std::isalnum(c) && c != '+' && c != '-' && c != '.') return false;
    }
    // "C://x" is a Windows path, not a URL with scheme "C".
    return sep > 1;
}

/**
 * @brief The path of a file:// URI, decoded.
 * @param uri The URI.
 * @return The path, or "".
 */
std::string PlaylistText::fileUriToPath(const std::string& uri)
{
    if (plLower(uri.substr(0, 7)) != "file://") return std::string();
    std::string rest = uri.substr(7);
    // file://localhost/x and file:///x name the same file.
    if (plLower(rest.substr(0, 10)) == "localhost/") rest.erase(0, 9);
    std::string path = percentDecode(rest);
#ifdef _WIN32
    // file:///C:/x -> C:/x
    if (path.size() >= 3 && path[0] == '/' && std::isalpha(static_cast<unsigned char>(path[1])) && path[2] == ':') {
        path.erase(0, 1);
    }
#endif
    return path;
}

/**
 * @brief Decode %XX escapes.
 * @param text The text.
 * @return It, decoded.
 */
std::string PlaylistText::percentDecode(const std::string& text)
{
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '%' && i + 2 < text.size() &&
            std::isxdigit(static_cast<unsigned char>(text[i + 1])) &&
            std::isxdigit(static_cast<unsigned char>(text[i + 2]))) {
            out += static_cast<char>(std::stoi(text.substr(i + 1, 2), nullptr, 16));
            i += 2;
        } else {
            out += text[i];
        }
    }
    return out;
}

} // namespace Utility
} // namespace Core
} // namespace PsyMP3
