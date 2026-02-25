#include <cstdio>
#include <cstdlib>
#include <thread>
#include <atomic>
#include <chrono>
#include "CharIS/include/is_text_document.h"
#include "CharIS/include/is_text_renderer.h"
#include "CharIS/include/is_font_engine.h"
#include "CharIS/include/is_font_provider.h"
#include "os_base.h"
#include "demo_graphics.h"



#ifdef _WIN32
#include <Windows.h>
#endif

extern const char16_t* const gs_test_text;


struct UserData {
    CharIS::IISTextDocument* document = nullptr;

    bool draw_caret = true;
    CharIS::fppoint_t caret_point = {};
    CharIS::fpsize_t caret_size = {};

    void UpdateCaret(const CharIS::HitTestCtx& ctx) {
        caret_point = ctx.caret1;
        CharIS::fp26dot6_t w = ctx.caret2.x - ctx.caret1.x;
        CharIS::fp26dot6_t h = ctx.caret2.y - ctx.caret1.y;
        caret_size.width = w;
        caret_size.height = h;
    }

    void UpdateCaret() {
        const auto doc = document;

        CharIS::TextSelection sel = {};
        uint32_t count = 1;
        doc->GetSelection(&sel, count);

        CharIS::HitTestCtx ctx;
        doc->HitTest(sel.caret, ctx);

        this->UpdateCaret(ctx);
    }

    void SetCaretToPos(int x, int y, bool keepAnchor)
    {
        const auto doc = document;
        const auto ptx = float(x);
        const auto pty = float(y);
        CharIS::fp26dot6_t fpx = CharIS::fp26dot6_t(ptx * 64.f);
        CharIS::fp26dot6_t fpy = CharIS::fp26dot6_t(pty * 64.f);

        CharIS::HitTestCtx ctx;
        doc->HitTest({ fpx, fpy }, ctx);

        const auto code = doc->SetSelection(CharIS::SELECTION_MODE_SET, ctx.postion, keepAnchor);
        this->UpdateCaret(ctx);
        if (code == CharIS::CODE_OK) {
            draw_caret = true;
        }
    }

};


WindowEvents InitEvent(UserData* user_data) {

    return WindowEvents{
        nullptr,
        nullptr,
        [](void* ud, bool draw) {
            const auto self = static_cast<UserData*>(ud);
            self->draw_caret = draw;
            self->UpdateCaret();
        },
        [](void* ud, int x, int y) {
            static_cast<UserData*>(ud)->SetCaretToPos(x, y, false);
        },
        nullptr,
        [](void* ud, int x, int y) {
            static_cast<UserData*>(ud)->SetCaretToPos(x, y, true);
        },
        user_data,
    };

};

#include <vector>

int main() {
#ifdef _WIN32
    ::SetProcessDPIAware();
    const auto hr = ::CoInitialize(nullptr);
    assert(SUCCEEDED(hr));
#endif

    UserData user_data = { };

    WindowEvents initEvent = InitEvent(&user_data);

    IDemoWindow* win = CreateDemoWindow();
    IDemoGraphics* grahics = CreateDemoGraphicsD3D11();
    WindowConfig config;
    if (!win->Create(&config, &initEvent)) {
        win->Release();
        return -1;
    }

    HWND hwnd = static_cast<HWND>(win->GetNativeHandle());
    grahics->Initialize(hwnd);

    CharIS::IISFontProvider* provider = nullptr;
    CharIS::IISFontEngine* engine = nullptr;
    CharIS::IISTextRenderer* renderer = nullptr;
    CharIS::IISTextDocument* document = nullptr;

    CharisCreateFontProvider(&provider);
    CharisCreateFontEngine(&engine, provider);
    engine->SetGlobalFallbackList(u"Microsoft YaHei, Segoe UI Emoji");


    {
        CharIS::IISFontFace* face2 = nullptr;
        engine->MatchFontFace(&face2, u"Courier New");
        CharIS::IISFontFaceGlyph* glyph = nullptr;
        face2->Lock(u'大', &glyph);
        face2->Unlock(glyph);
        //std::vector<std::thread> threads;
        //for (int i = 0; i != 5; ++i) {
        //    threads.emplace_back([face2]{
        //        for (int i = 1; i != 100000; ++i) {
        //            CharIS::IISFontFaceGlyph* glyph = nullptr;
        //            if (CharIS::Success(face2->Lock(i, &glyph))) {
        //                CharIS::BitmapInfo info;
        //                glyph->RenderToBitmap(65 << 6, &info);
        //                face2->Unlock(glyph);
        //            }
        //        }
        //        
        //    });
        //}

        //for (auto& t : threads) t.join();

        face2->Dispose();
    }

    //engine->SetGlobalFallbackList(u"Microsoft YaHei, Segoe UI Emoji");

    {
        //CharIS::IISFontFace* face = nullptr;
        //engine->MatchFontFace(&face, u"KaiTi");
        //CharIS::SafeDispose(face);
    }

    //const auto fontName = u"Courier New";
    //const auto fontName = u"Arial";
    const auto fontName = u"Microsoft YaHei";
    //const auto fontName = u"Source Han Serif SC VF";
    //const auto fontName = u"KaiTi";
    //const auto fontName = u"FangSong";
    // 
    //const auto fontName = u"Kunstler Script";


    CharisCreateTextRenderer(&renderer, engine, grahics, { 2048, 50 * 1000, false, true, 1 });

    CharisCreateTextDocument(&document, renderer, fontName, 20 << 6);
    //document->InsertText(0, u"支鹰持Hello, world! \n支鹰持 + 789456132");
    //document->InsertText(0, u"ス aHea+,llo");
    document->InsertText(0, gs_test_text);

    std::vector<uint64_t> ids;
    const auto s = win->GetWindowSize();
    grahics->SetViewport(0, 0, s.width, s.height);

    int counter = 0;
    user_data.document = document;

    using Clock = std::chrono::steady_clock;
    auto fps_interval_start = Clock::now();
    int frames_in_interval = 0;
    constexpr auto fps_interval_duration = std::chrono::milliseconds(500);

    const auto render = [=, &user_data, &ids, &counter](std::vector<uint64_t>& ids_ref) {
        ++counter;
        grahics->BeginFrame(ids_ref);
        //grahics->Clear(0.0f, 0.0f, 0.0f, 1.0f);
        grahics->Clear(1.0f, 1.0f, 1.0f, 1.0f);
        renderer->Update(ids_ref.data(), ids_ref.size());

        //for (int i = 0; i != 99; ++i) {
        //    document->Draw(nullptr, {}, nullptr);
        //}
        //grahics->Clear(0.0f, 0.0f, 0.0f, 1.0f);

        const auto draw = document->Draw(nullptr, {}, nullptr);

        if (draw.count == draw.drawn) {
            ::Sleep(0);
        }



        if (user_data.draw_caret) {
            auto caret_size = user_data.caret_size;
            caret_size.width = 1 << 6;
            grahics->FillRect(0xFFFFFFFFu, nullptr, user_data.caret_point, caret_size);
        }

        grahics->EndFrame();
    };

    while (!win->IsQuitRequested()) {
        while (win->PumpEvents());
        render(ids);

        ++frames_in_interval;
        const auto now = Clock::now();
        if (now - fps_interval_start >= fps_interval_duration) {
            const double elapsed_sec = std::chrono::duration<double>(now - fps_interval_start).count();
            const double fps = (elapsed_sec > 0.0) ? (frames_in_interval / elapsed_sec) : 0.0;
            char title_buf[128];
            std::snprintf(title_buf, sizeof(title_buf), "Window - %.1f FPS", fps);
            win->SetTitle(title_buf);
            frames_in_interval = 0;
            fps_interval_start = now;
        }
    }

    CharIS::SafeDispose(document);
    CharIS::SafeDispose(renderer);
    CharIS::SafeDispose(engine);
    CharIS::SafeDispose(provider);

    grahics->Release();
    win->Release();

#ifdef _WIN32
    ::CoUninitialize();
#endif

    return 0;
}


#if 1
const char16_t* const gs_test_text = uR"(このチュートリアルはVulkanグラフィックスとコンピュートAPIの基本的な使い方を教えます。
VulkanはKhronosグループ(OpenGLで知られています)による新しいAPIで、現代のグラフィックスカードのより良い抽象化を提供します。
この新しいインターフェースはあなたのアプリケーションが何をしようとしているかをより良く記述できるようにし、より良いパフォーマンスに導き、
そしてOpenGLやDirect3DといったAPIと比べてドライバーの振る舞いに対して驚くことを少なくします。
Vulkanの背後にあるアイデアは Direct3D 12 や Metal と似ていますが、Vulkanはクロスプラットフォームであり、
Windows,Linux,Androidで同時に開発できるというアドバンテージを持っています。

しかし、この利益の対価として、あなたはかなり冗長なAPIと一緒に働かなければいけません。すべてのグラフィックスAPIに関する詳細は、
あなたのアプリケーションによって最初から設定される必要があります。それには、フレームバッファの作成やバッファやテクスチャ画像といったオブジェクトのメモリ管理を含みます。
グラフィックスドライバーがやる仕事は少なくなりますが、それは正しい振る舞いをするためにはあなたのアプリケーションがやらなければいけない仕事が多くなるということを意味します。

ここで読み取って欲しいメッセージは、Vulkanは万人のためのものではないということです。高パフォーマンスコンピュータグラフィックスに熱中し、
それについてなにかしたいと願っているプログラマーをターゲットにしています。もしあなたがコンピュータグラフィックスよりゲーム開発に興味があるなら、
OpenGLやDirect3Dを支持したいかもしれません。そして、それがすぐにVulkanに賛成して避難されるというわけではありません1。もうひとつの代案は、
Unreal Engine や Unity といったエンジンを使うことで、高レベルAPIを公開している場合はVulkanを使うことができます。

The ID2D1GeometrySink interface extends the ID2D1SimplifiedGeometrySink interface to add support for arcs and quadratic beziers, 
as well as functions for adding single lines and cubic beziers.

A geometry sink consists of one or more figures. Each figure is made up of one or more line, curve, or arc segments. To create a figure, 
call the BeginFigure method, specify the figure's start point, and then use its Add methods (such as AddLine and AddBezier) to add segments.
When you are finished adding segments, call the EndFigure method. You can repeat this sequence to create additional figures.
When you are finished creating figures, call the Close method.


什么是Lorem Ipsum?
Lorem Ipsum，也称乱数假文或者哑元文本， 是印刷及排版领域所常用的虚拟文字。由于曾经一台匿名的打印机刻意打乱了一盒印刷字体从而造出一本字体样品书，
Lorem Ipsum从西元15世纪起就被作为此领域的标准文本使用。它不仅延续了五个世纪，还通过了电子排版的挑战，其雏形却依然保存至今。
在1960年代，”Leatraset”公司发布了印刷着Lorem Ipsum段落的纸张，从而广泛普及了它的使用。最近，计算机桌面出版软件”Aldus PageMaker”也通过同样的方式使Lorem Ipsum落入大众的视野。

我们为何用它？
无可否认，当读者在浏览一个页面的排版时，难免会被可阅读的内容所分散注意力。Lorem Ipsum的目的就是为了保持字母多多少少标准及平均的分配，
而不是“此处有文本，此处有文本”，从而让内容更像可读的英语。如今，很多桌面排版软件以及网页编辑用Lorem Ipsum作为默认的示范文本，搜一搜“Lorem Ipsum”就能找到这些网站的雏形。
这些年来Lorem Ipsum演变出了各式各样的版本，有些出于偶然，有些则是故意的（刻意的幽默之类的）。


它起源于哪里？
恰恰与流行观念相反，Lorem Ipsum并不是简简单单的随机文本。它追溯于一篇公元前45年的经典拉丁著作，从而使它有着两千多年的岁数。
弗吉尼亚州Hampden-Sydney大学拉丁系教授Richard McClintock曾在Lorem Ipsum段落中注意到一个涵义十分隐晦的拉丁词语，“consectetur”，
通过这个单词详细查阅跟其有关的经典文学著作原文，McClintock教授发掘了这个不容置疑的出处。Lorem Ipsum始于西塞罗(Cicero)在公元前45年作
的“de Finibus Bonorum et Malorum”（善恶之尽）里1.10.32 和1.10.33章节。这本书是一本关于道德理论的论述，曾在文艺复兴时期非常流行。
Lorem Ipsum的第一行”Lorem ipsum dolor sit amet..”节选于1.10.32章节。

以下展示了自1500世纪以来使用的标准Lorem Ipsum段落，西塞罗笔下“de Finibus Bonorum et Malorum”章节1.10.32 ， 1.10.33的原著作，
以及其1914年译自H. Rackham的英文版本。

我能从哪里获取？
如今互联网提供各种各样版本的Lorem Ipsum段落，但是大多数都多多少少出于刻意幽默或者其他随机插入的荒谬单词而被篡改过了。
如果你想取用一段Lorem Ipsum，请确保段落中不含有令人尴尬的不恰当内容。所有网上的Lorem Ipsum生成器都倾向于在必要时重复预先准备的部分，
然而这个生成器则是互联网上首个确切的生成器。它使用由超过200个拉丁单词所构造的词典，结合了几个模范句子结构，来生成看起来恰当的Lorem Ipsum。
因此，生成出的结果无一例外免于重复，刻意的幽默，以及非典型的词汇等等。

)";
#else

const char16_t* const gs_test_text = uR"(g什么是Lorem Ipsum?
Lorem Ipsum，也称乱数假文或者哑元文本， 
是印刷及排版领域所常用的虚拟文字。
由于曾经一台匿名的打印机刻意打乱了一盒
印刷字体从而造出一本字体样品书，
Lorem Ipsum从西元15世纪起就被作为此领域
的标准文本使用。它不仅延续了五个世纪，
还通过了电子排版的挑战，其雏形却依然保
存至今。
在1960年代，”Leatraset”公司发布了印刷着
Lorem Ipsum段落的纸张，从而广泛普及了它的使用。
最近，计算机桌面出版软件”Aldus PageMaker”
也通过同样的方式使Lorem Ipsum落入大众的视野。
)";

#endif