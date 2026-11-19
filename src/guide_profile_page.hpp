#pragma once
#include <string>
#include <string_view>

namespace ss {
enum class GuideProbePurpose { openExtensions, verifyConnector };
enum class GuideProbeProgress { waiting, matched, proceeding };

inline std::string guideProfilePage(std::string_view title, GuideProbePurpose purpose, bool english,
                                    bool light, bool reduced) {
    std::string page = english ? "<!doctype html><html lang=\"en\">" : "<!doctype html><html lang=\"ru\">";
    page += "<meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>";
    page += title;
    page += R"html(</title>
<style>
*{box-sizing:border-box}
:root{font-family:"Segoe UI",system-ui,sans-serif}
body{--bg:#0b1220;--panel:#131d2e;--line:#28374a;--text:#ecf4ff;--muted:#9aadc5;--accent:#78e0cb;--wash:#142f33;
margin:0;min-height:100vh;padding:clamp(18px,5vw,64px);color:var(--text);
background:radial-gradient(ellipse at 82% 18%,var(--wash),transparent 50%),var(--bg);color-scheme:dark}
body.light{--bg:#edf3fa;--panel:#fff;--line:#cedceb;--text:#172b43;--muted:#4c627b;--accent:#167c68;--wash:#d7eeec;color-scheme:light}
main{max-width:980px;margin:auto}
header{display:flex;gap:14px;align-items:center;justify-content:space-between;margin-bottom:clamp(24px,7vh,72px)}
.brand{display:flex;gap:12px;align-items:center;font-weight:700;letter-spacing:.04em}
.mark{width:38px;height:38px;display:grid;place-items:center;border:1px solid var(--accent);border-radius:12px;color:var(--accent)}
.local{font-size:12px;color:var(--muted);border:1px solid var(--line);border-radius:999px;padding:6px 12px}
.panel{background:var(--panel);border:1px solid var(--line);border-radius:26px;padding:clamp(20px,4vw,44px);
box-shadow:0 24px 70px #0b122019}
.hero{display:grid;grid-template-columns:minmax(0,1fr) minmax(160px,260px);gap:16px;align-items:center}
.eyebrow{color:var(--accent);font-size:12px;font-weight:700;letter-spacing:.11em;text-transform:uppercase}
h1{font-size:clamp(28px,5vw,54px);line-height:1.08;letter-spacing:-.035em;margin:12px 0}
p{margin:0;color:var(--muted);line-height:1.62}
.signal{display:block;width:min(100%,260px);margin:auto;color:var(--accent)}
.orbit{stroke-dasharray:6 8;transform-origin:121px 89px;animation:orbit 14s linear infinite}
.wave{transform-origin:121px 89px;animation:pulse 2.4s ease-out infinite}
.steps{list-style:none;margin:clamp(22px,4vw,42px) 0 0;padding:0;display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:10px}
.step{padding:14px 13px;min-height:76px;border:1px solid var(--line);border-radius:14px;display:flex;gap:10px;align-items:center;color:var(--muted)}
.number{flex:none;display:grid;place-items:center;width:28px;height:28px;border-radius:50%;border:1px solid var(--line);font-size:12px}
.step b{font-size:14px;font-weight:600}
.step.done,.step.active{color:var(--text)}
.step.done .number{color:var(--accent)}
.step.active{border-color:var(--accent)}
.step.active .number{border-color:var(--accent);color:var(--accent)}
.status{min-height:48px;margin-top:16px;display:flex;align-items:center;gap:10px;color:var(--accent);font-size:14px}
.status:before{content:"";width:7px;height:7px;border-radius:50%;background:currentColor;flex:none}
footer{margin:16px 0 0;color:var(--muted);font-size:13px;line-height:1.5}
html[lang=ru] .en,html[lang=en] .ru,.opening .verify,.checking .open{display:none}
@keyframes orbit{to{transform:rotate(360deg)}}
@keyframes pulse{0%{transform:scale(.55);opacity:.68}80%,100%{transform:scale(1.27);opacity:0}}
@media(max-width:670px){.hero{grid-template-columns:1fr}.signal{width:145px;order:-1;margin:0}.steps{grid-template-columns:1fr}.step{min-height:44px}}
@media(prefers-reduced-motion:reduce){*,*::before{animation:none!important}}
body.reduced *,body.reduced *::before,body.idle .orbit,body.idle .wave{animation:none}
</style>)html";
    page += "<body class=\"";
    page += purpose == GuideProbePurpose::openExtensions ? "opening" : "checking";
    if (light) page += " light";
    if (reduced) page += " reduced";
    page += R"html("><main>
<header><div class="brand"><span class="mark" aria-hidden="true"><svg width="18" height="18" viewBox="0 0 18 18" fill="currentColor"><path d="M6 3.6a.7.7 0 0 0-1 .6v9.6a.7.7 0 0 0 1 .6l8-4.8a.7.7 0 0 0 0-1.2z"/></svg></span>StreamShelf</div>
<span class="local"><span class="ru">Локальная страница</span><span class="en">Local page</span></span></header>
<section class="panel">
<div class="hero">
<div><div class="eyebrow"><span class="ru">Мастер установки</span><span class="en">Extension setup</span></div>
<h1><span class="ru">Проверка браузера</span><span class="en">Checking your browser</span></h1>
<p class="open"><span class="ru">StreamShelf находит выбранное окно и готовит переход на страницу расширений.</span><span class="en">StreamShelf locates the selected window and prepares to open the extensions page.</span></p>
<p class="verify"><span class="ru">StreamShelf проверяет связь с расширением в выбранном профиле браузера.</span><span class="en">StreamShelf checks the connection to the extension in the selected browser profile.</span></p></div>
<svg class="signal" aria-hidden="true" viewBox="0 0 242 178" fill="none">
<circle class="orbit" cx="121" cy="89" r="73" stroke="currentColor" stroke-opacity=".48"/>
<circle class="wave" cx="121" cy="89" r="44" stroke="currentColor" stroke-opacity=".5" stroke-width="2"/>
<rect x="57" y="53" width="128" height="80" rx="16" fill="var(--panel)" stroke="currentColor" stroke-opacity=".66" stroke-width="2"/>
<path d="M57 74h128M70 64h3m5 0h3m5 0h3" stroke="currentColor" stroke-opacity=".55" stroke-width="2" stroke-linecap="round"/>
<path d="M106 102h7l5-11 6 21 5-10h7" stroke="currentColor" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"/>
</svg></div>
<ol class="steps">
<li class="step done"><span class="number" aria-hidden="true">✓</span><b><span class="ru">Локальная страница открыта</span><span class="en">Local page opened</span></b></li>
<li class="step active" aria-current="step"><span class="number" aria-hidden="true">2</span><b>
<span class="open"><span class="ru">Поиск окна браузера</span><span class="en">Locate browser window</span></span>
<span class="verify"><span class="ru">Ответ расширения</span><span class="en">Extension response</span></span></b></li>
<li class="step"><span class="number" aria-hidden="true">3</span><b>
<span class="open"><span class="ru">Переход к расширениям</span><span class="en">Open extensions page</span></span>
<span class="verify"><span class="ru">Подтверждение связи</span><span class="en">Confirm connection</span></span></b></li>
</ol>
<p id="status" class="status" role="status"><span class="ru">Ожидаю мастера StreamShelf…</span><span class="en">Waiting for StreamShelf…</span></p>
</section>
<footer><span class="ru">Действий не требуется. Если вкладка осталась открытой, вернитесь к мастеру.</span><span class="en">No action is needed. If this tab stays open, return to the setup guide.</span></footer>
</main>
<script>
(() => {
    const ru = document.documentElement.lang === 'ru';
    const opening = document.body.classList.contains('opening');
    const status = document.getElementById('status');
    const steps = document.querySelectorAll('.step');
    const messages = opening
        ? (ru ? ['Мастер ищет выбранное окно…','Окно найдено. Проверяю открытые вкладки…','Перехожу к странице расширений…']
              : ['Locating the selected window…','Window found. Checking open tabs…','Opening the extensions page…'])
        : (ru ? ['Ожидаю ответ расширения…','Расширение ответило. Проверяю связь…','Связь подтверждена. Возвращаюсь к мастеру…']
              : ['Waiting for the extension…','Extension responded. Checking the connection…','Connection confirmed. Returning to the guide…']);
    const deadline = performance.now() + 21000;
    let last = -1, failures = 0;
    const render = stage => {
        if (stage === last) return;
        last = stage;
        status.textContent = messages[stage];
        steps[1].classList.toggle('done', stage > 0);
        steps[1].classList.toggle('active', stage === 0);
        steps[1].querySelector('.number').textContent = stage > 0 ? '✓' : '2';
        if (stage > 0) steps[1].removeAttribute('aria-current');
        steps[2].classList.toggle('active', stage > 0 && (opening || stage < 2));
        steps[2].classList.toggle('done', !opening && stage === 2);
        steps[2].querySelector('.number').textContent = !opening && stage === 2 ? '✓' : '3';
        if (stage > 0 && (opening || stage < 2)) steps[2].setAttribute('aria-current', 'step');
        else steps[2].removeAttribute('aria-current');
    };
    const finish = () => {
        document.body.classList.add('idle');
        for (const step of steps) { step.classList.remove('active'); step.removeAttribute('aria-current'); }
    };
    const poll = async () => {
        try {
            const response = await fetch(location.pathname + '/status', {cache:'no-store', signal:AbortSignal.timeout(900)});
            if (!response.ok) throw Error('status');
            const {stage} = await response.json();
            if (!Number.isInteger(stage) || stage < 0 || stage >= messages.length) throw Error('stage');
            render(stage);
            failures = 0;
            if (!opening && stage === 2) { finish(); return; }
        } catch { ++failures; }
        if (failures >= 3 || performance.now() >= deadline) {
            finish();
            status.textContent = ru ? 'Ожидание закончилось. Вернитесь к мастеру StreamShelf.'
                                    : 'The wait has ended. Return to the StreamShelf guide.';
            return;
        }
        setTimeout(poll, 450);
    };
    requestAnimationFrame(() => requestAnimationFrame(poll));
})();
</script></body></html>)html";
    return page;
}
}
