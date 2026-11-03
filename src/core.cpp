#include "core.hpp"
#include <iomanip>
#include <set>
namespace ss {
std::string str(const Json &j, const char *k, const std::string &f) {
    auto i = j.find(k);
    if (i == j.end() || i->is_null())
        return f;
    if (i->is_string())
        return i->get<std::string>();
    return i->dump();
}
double number(const Json &j, const char *k, double f) {
    auto i = j.find(k);
    return i != j.end() && i->is_number() ? i->get<double>() : f;
}
bool httpUrl(const std::string &s) {
    return s.size() < 32768 && (s.rfind("https://", 0) == 0 || s.rfind("http://", 0) == 0) &&
           s.find_first_of("\r\n\t ") == std::string::npos &&
           s.find('@', s.find("://") + 3) == std::string::npos;
}
Analysis parseAnalysis(const Json &j, const std::string &url) {
    if (!j.is_object())
        throw std::runtime_error("Некорректный ответ yt-dlp");
    if (j.contains("entries"))
        throw std::runtime_error("Выберите отдельный ролик вместо плейлиста");
    Analysis a{str(j, "title", "Медиаконтент"), url, j, {}};
    auto add = [&](Media m) {
        m.key = m.category + "|" + m.id + "|" + m.url + "|page:" + url;
        m.page = url;
        if(m.thumbnail.empty())m.thumbnail=str(j,"thumbnail");
        a.items.push_back(std::move(m));
    };
    auto formats = j.value("formats", Json::array());
    if (formats.empty() && j.contains("url"))
        formats.push_back(j);
    for (auto &f : formats) {
        Media m;
        m.id = str(f, "format_id", "direct");
        m.title = str(f, "format_note", m.id);
        m.url = str(f, "url");
        m.ext = str(f, "ext");
        m.protocol = str(f, "protocol");
        m.vcodec = str(f, "vcodec");
        m.acodec = str(f, "acodec");
        m.quality = str(f, "resolution");
        if (m.quality.empty() && number(f, "height") > 0)
            m.quality = std::to_string(int(number(f, "height"))) + "p";
        m.language = str(f, "language");
        m.size = number(f, "filesize", number(f, "filesize_approx"));
        m.approx = !f.contains("filesize") || f["filesize"].is_null();
        m.fps = number(f, "fps");
        m.rate = number(f, "tbr");
        m.drm = f.contains("has_drm") && f["has_drm"].is_boolean() && f["has_drm"].get<bool>();
        m.format = true;
        m.videoOnly = !m.vcodec.empty() && m.vcodec != "none" && m.acodec == "none";
        m.category = m.drm                ? "Защищённые"
                     : m.vcodec == "none" && !m.acodec.empty() && m.acodec != "none" ? "Аудио"
                     : m.vcodec.empty() || m.vcodec == "none" ? "Требуют проверки"
                     : m.videoOnly        ? "Видео без звука"
                     : m.acodec.empty()   ? "Видео · звук не проверен"
                                          : "Видео";
        m.data = f;
        add(m);
    }
    for (auto key : {"subtitles", "automatic_captions"})
        if (j.contains(key) && j[key].is_object())
            for (auto &[lang, variants] : j[key].items())
                if (variants.is_array())
                    for (auto &v : variants) {
                        Media m;
                        m.automatic = std::string(key) == "automatic_captions";
                        m.id = std::string(key) + ":" + lang + ":" + str(v, "ext");
                        m.category = m.automatic ? "Автосубтитры" : "Субтитры";
                        m.language = lang;
                        m.ext = str(v, "ext");
                        if (m.ext.empty() || m.ext.size() > 12 ||
                            !std::all_of(m.ext.begin(), m.ext.end(),
                                         [](unsigned char c) { return std::isalnum(c); }))
                            m.ext = "bin";
                        m.url = str(v, "url");
                        m.title = lang + " · " + m.ext;
                        m.data = v;
                        add(m);
                    }
    if (j.contains("thumbnails") && j["thumbnails"].is_array())
        for (auto &t : j["thumbnails"]) {
            Media m;
            m.category = "Превью";
            m.id = "thumb:" + str(t, "id", std::to_string(a.items.size()));
            m.url = str(t, "url");
            m.thumbnail=m.url;
            m.title = str(t, "id", "Обложка");
            m.quality =
                std::to_string(int(number(t, "width"))) + "×" + std::to_string(int(number(t, "height")));
            m.ext = "jpg";
            auto path = m.url.substr(0, m.url.find('?'));
            auto dot = path.find_last_of('.');
            if (dot != path.npos) {
                auto ext = path.substr(dot + 1);
                if (ext == "png" || ext == "webp" || ext == "avif" || ext == "jpeg" || ext == "jpg")
                    m.ext = ext;
            }
            m.data = t;
            add(m);
        }
    for (auto name : {"metadata", "description", "chapters"})
        if (std::string(name) == "metadata" || (j.contains(name) && !j[name].is_null())) {
            Media m;
            m.id = name;
            m.category = "Данные";
            m.ext = std::string(name) == "description" ? "txt" : "json";
            m.title = std::string(name) == "metadata"   ? "Метаданные JSON"
                      : std::string(name) == "chapters" ? "Главы"
                                                        : "Описание";
            m.data = j;
            add(m);
        }
    return a;
}
std::wstring quoteWide(const std::wstring &s) {
    std::wstring out = L"\"";
    size_t n = 0;
    for (wchar_t c : s) {
        if (c == L'\\') {
            n++;
            continue;
        }
        if (c == L'\"') {
            out.append(n * 2 + 1, L'\\');
            out += c;
        } else {
            out.append(n, L'\\');
            out += c;
        }
        n = 0;
    }
    out.append(n * 2, L'\\');
    return out + L'\"';
}
std::string redact(std::string s) {
    static const std::regex header(R"((Authorization|Cookie|Set-Cookie|X-Api-Key)\s*[:=][^\r\n]*)",
                                   std::regex::icase),
        url(R"((https?://[^\s?"<>]+)\?[^\s"<>]*)", std::regex::icase);
    s = std::regex_replace(s, header, "$1: [скрыто]");
    return std::regex_replace(s, url, "$1?[скрыто]");
}
std::string cookies(const Json &a) {
    if (!a.is_array())
        throw std::runtime_error("Некорректный снимок cookies");
    std::string out = "# Netscape HTTP Cookie File\n";
    for (auto &c : a) {
        if (c.value("partitioned", false))
            throw std::runtime_error("Partitioned cookies нельзя безопасно передать через Netscape-файл");
        std::string d = str(c, "domain"), p = str(c, "path", "/"), n = str(c, "name"), v = str(c, "value");
        for (auto &s : {d, p, n, v})
            if (s.find_first_of("\t\r\n") != s.npos)
                throw std::runtime_error("Недопустимые данные cookies");
        if (d.empty())
            continue;
        out += (c.value("httpOnly", false) ? "#HttpOnly_" : "") + d + "\t" +
               (d[0] == '.' ? "TRUE" : "FALSE") + "\t" + p + "\t" +
               (c.value("secure", false) ? "TRUE" : "FALSE") + "\t" +
               std::to_string(static_cast<int64_t>(number(c, "expirationDate"))) + "\t" + n + "\t" + v + "\n";
    }
    return out;
}
std::string validateCookies(const std::string &s) {
    if (s.size() > 4 * 1024 * 1024 || s.find('\0') != s.npos)
        throw std::runtime_error("Некорректный cookies.txt");
    std::istringstream in(s);
    std::string line;
    bool found = false;
    while (std::getline(in, line)) {
        if (line.empty() || (line[0] == '#' && line.rfind("#HttpOnly_", 0) != 0))
            continue;
        if (std::count(line.begin(), line.end(), '\t') != 6)
            throw std::runtime_error("Ожидается формат Netscape cookies.txt");
        found = true;
    }
    if (!found)
        throw std::runtime_error("Файл cookies пуст");
    return s;
}
std::string sizeText(double n, bool approx) {
    if (n <= 0)
        return "Неизвестен";
    static const char *u[] = {"Б", "КБ", "МБ", "ГБ", "ТБ"};
    int i = 0;
    while (n >= 1024 && i < 4) {
        n /= 1024;
        i++;
    }
    std::ostringstream o;
    o << std::fixed << std::setprecision(i ? 1 : 0) << n << ' ' << u[i] << (approx ? " ~" : "");
    return o.str();
}
std::vector<uint32_t> codepoints(const std::string &s) {
    std::vector<uint32_t> o;
    for (size_t i = 0; i < s.size();) {
        uint8_t b = s[i++];
        uint32_t v = b;
        int n = 0;
        if (b >= 0xf0) {
            v = b & 7;
            n = 3;
        } else if (b >= 0xe0) {
            v = b & 15;
            n = 2;
        } else if (b >= 0xc0) {
            v = b & 31;
            n = 1;
        } else if (b >= 128) {
            o.push_back(0xfffd);
            continue;
        }
        bool good = true;
        for (int j = 0; j < n; j++) {
            if (i >= s.size() || (uint8_t(s[i]) & 0xc0) != 0x80) {
                good = false;
                break;
            }
            v = (v << 6) | (uint8_t(s[i++]) & 63);
        }
        o.push_back(good ? v : 0xfffd);
    }
    return o;
}
std::string sanitizeMetadata(const Json &j) {
    std::function<Json(const Json &)> clean = [&](const Json &v) -> Json {
        if (v.is_object()) {
            Json out = Json::object();
            for (auto &[k, x] : v.items()) {
                std::string l = k;
                std::transform(l.begin(), l.end(), l.begin(), ::tolower);
                if (l.find("url") != l.npos || l.find("cookie") != l.npos || l.find("header") != l.npos ||
                    l.find("token") != l.npos || l.find("password") != l.npos)
                    continue;
                out[k] = clean(x);
            }
            return out;
        }
        if (v.is_array()) {
            Json out = Json::array();
            for (auto &x : v)
                out.push_back(clean(x));
            return out;
        }
        return v.is_string() ? Json(redact(v.get<std::string>())) : v;
    };
    return clean(j).dump(2);
}
std::string dataSourceContent(const Media &item) {
    if (!item.data.is_object()) throw std::runtime_error("Данные источника недоступны. Повторите анализ страницы.");
    if (item.id == "metadata") return sanitizeMetadata(item.data);
    if (item.id == "description") return redact(str(item.data, "description"));
    if (item.id == "chapters") return item.data.value("chapters", Json::array()).dump(2);
    throw std::runtime_error("Неизвестный источник данных");
}
} // namespace ss
