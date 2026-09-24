/*
 * Leapdesk KVM -- mouse and keyboard sharing utility
 * Copyright (C) 2026 Leapdesk KVM contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "platform/MSWindowsClipboardFilesConverter.h"
#include "platform/MSWindowsFilePaste.h"
#include "platform/MSWindowsTaildrop.h"
#include "inputleap/Clipboard.h"
#include "inputleap/FileClip.h"

#include <gtest/gtest.h>

#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <mutex>
#include <thread>

namespace inputleap {

namespace {

// a folder of files that goes away with the test
class TempFolder {
public:
    TempFolder()
    {
        static int count = 0;
        wchar_t base[MAX_PATH];
        GetTempPathW(MAX_PATH, base);
        path_ = std::wstring(base) + L"leapdesk-file-paste-" +
                std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(++count);
        CreateDirectoryW(path_.c_str(), nullptr);
    }
    ~TempFolder()
    {
        // SHFileOperation wants a double-null-terminated list
        std::wstring from = path_ + L'\0';
        SHFILEOPSTRUCTW operation{};
        operation.wFunc = FO_DELETE;
        operation.pFrom = from.c_str();
        operation.fFlags = FOF_NO_UI;
        SHFileOperationW(&operation);
    }

    std::wstring file(const std::wstring& name, const std::string& contents)
    {
        std::wstring path = path_ + L"\\" + name;
        std::ofstream(path, std::ios::binary) << contents;
        return path;
    }
    std::wstring folder(const std::wstring& name)
    {
        std::wstring path = path_ + L"\\" + name;
        CreateDirectoryW(path.c_str(), nullptr);
        return path;
    }
    const std::wstring& path() const { return path_; }

private:
    std::wstring path_;
};

std::string read(const std::wstring& path)
{
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

bool exists(const std::wstring& path)
{
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// CF_HDROP data as Explorer puts it on the clipboard
HGLOBAL make_drop(const std::vector<std::wstring>& paths)
{
    return MSWindowsFilePaste::make_drop(paths);
}

// what a program receives from \p command_line
std::vector<std::wstring> split(const std::wstring& command_line)
{
    int count = 0;
    wchar_t** arguments = CommandLineToArgvW(command_line.c_str(), &count);
    std::vector<std::wstring> result(arguments, arguments + count);
    LocalFree(arguments);
    return result;
}

// sends \p path to \p address with Taildrop and returns every status reported
std::vector<FilePasteStatus> send_with_taildrop(const std::wstring& path,
                                                const std::string& address)
{
    std::mutex mutex;
    std::condition_variable reported;
    std::vector<FilePasteStatus> statuses;
    auto finished = [&statuses]() {
        return !statuses.empty() && statuses.back().state != FilePasteState::SENDING;
    };

    MSWindowsTaildrop taildrop([&](const FilePasteStatus& status) {
        std::lock_guard<std::mutex> lock(mutex);
        statuses.push_back(status);
        reported.notify_all();
    });
    EXPECT_TRUE(taildrop.send("0123456789abcdef", {path}, {"leapdesk-test-file.txt"}, address));

    std::unique_lock<std::mutex> lock(mutex);
    EXPECT_TRUE(reported.wait_for(lock, std::chrono::seconds(60), finished));
    return statuses;
}

} // namespace

TEST(MSWindowsFilePasteTests, filesConverter_describesCopiedFiles)
{
    TempFolder folder;
    std::wstring report = folder.file(L"report.pdf", "12345");
    std::wstring accented = folder.file(L"na\u00efve \u2713.txt", "");
    HGLOBAL drop = make_drop({report, accented});

    MSWindowsClipboardFilesConverter converter;
    EXPECT_EQ(IClipboard::kFiles, converter.getFormat());
    EXPECT_EQ(static_cast<UINT>(CF_HDROP), converter.getWin32Format());

    EXPECT_EQ((std::vector<std::wstring>{report, accented}),
              MSWindowsClipboardFilesConverter::get_paths(drop));

    FileClip clip;
    ASSERT_TRUE(FileClip::unmarshall(converter.toIClipboard(drop), clip));
    ASSERT_EQ(2u, clip.files.size());
    EXPECT_EQ("report.pdf", clip.files[0].name);
    EXPECT_EQ(5u, clip.files[0].size);
    EXPECT_NE(0u, clip.files[0].modified);
    EXPECT_EQ("na\xC3\xAFve \xE2\x9C\x93.txt", clip.files[1].name);
    EXPECT_EQ(0u, clip.files[1].size);

    // the source recomputes the id from its clipboard when asked to send
    FileClip again;
    ASSERT_TRUE(MSWindowsClipboardFilesConverter::make_file_clip({report, accented}, again));
    EXPECT_EQ(clip.id, again.id);

    GlobalFree(drop);
}

TEST(MSWindowsFilePasteTests, filesConverter_changedFileGetsNewId)
{
    TempFolder folder;
    std::wstring path = folder.file(L"notes.txt", "one");
    FileClip before, after;
    ASSERT_TRUE(MSWindowsClipboardFilesConverter::make_file_clip({path}, before));
    folder.file(L"notes.txt", "one two");
    ASSERT_TRUE(MSWindowsClipboardFilesConverter::make_file_clip({path}, after));
    EXPECT_NE(before.id, after.id);
}

TEST(MSWindowsFilePasteTests, filesConverter_leavesOutFoldersAndMissingFiles)
{
    TempFolder folder;
    std::wstring file = folder.file(L"file.txt", "x");
    std::wstring subfolder = folder.folder(L"subfolder");
    MSWindowsClipboardFilesConverter converter;

    HGLOBAL with_folder = make_drop({file, subfolder});
    EXPECT_EQ("", converter.toIClipboard(with_folder));
    GlobalFree(with_folder);

    HGLOBAL missing = make_drop({file + L".gone"});
    EXPECT_EQ("", converter.toIClipboard(missing));
    GlobalFree(missing);

    FileClip clip;
    EXPECT_FALSE(MSWindowsClipboardFilesConverter::make_file_clip({}, clip));
}

TEST(MSWindowsFilePasteTests, filesConverter_doesNotOfferOtherScreensFiles)
{
    MSWindowsClipboardFilesConverter converter;
    EXPECT_EQ(nullptr, converter.fromIClipboard(FileClip().marshall()));
}

TEST(MSWindowsFilePasteTests, taildrop_quotesArgumentsForTheProgram)
{
    for (const std::wstring& argument : {std::wstring(L"plain"), std::wstring(L""),
                                         std::wstring(L"with space"), std::wstring(L"tab\there"),
                                         std::wstring(L"quote\"inside"),
                                         std::wstring(L"C:\\trailing\\"),
                                         std::wstring(L"C:\\with space\\"),
                                         std::wstring(L"back\\\\\"slashes"),
                                         std::wstring(L"na\u00efve \u2713")}) {
        std::vector<std::wstring> received =
            split(L"program.exe " + MSWindowsTaildrop::quote_argument(argument));
        ASSERT_EQ(2u, received.size()) << argument;
        EXPECT_EQ(argument, received[1]);
    }
}

TEST(MSWindowsFilePasteTests, taildrop_commandLineSendsOneFile)
{
    std::wstring program = L"C:\\Program Files\\Tailscale\\tailscale.exe";
    std::wstring path = L"C:\\Users\\me\\My Documents\\report \"final\".pdf";
    std::string name = "leapdesk-0123456789abcdef-1-report \"final\".pdf";

    std::vector<std::wstring> received =
        split(MSWindowsTaildrop::command_line(program, path, name, "100.81.171.127"));

    EXPECT_EQ((std::vector<std::wstring>{program, L"file", L"cp", L"--update-interval=0",
                                         L"--name=leapdesk-0123456789abcdef-1-report \"final\".pdf",
                                         path, L"100.81.171.127:"}),
              received);
}

TEST(MSWindowsFilePasteTests, taildrop_reportsWhyItFailed)
{
    if (MSWindowsTaildrop::find_program().empty()) {
        GTEST_SKIP() << "Tailscale is not installed";
    }

    // no machine has this name, so tailscale refuses before sending anything
    TempFolder folder;
    std::vector<FilePasteStatus> statuses =
        send_with_taildrop(folder.file(L"file.txt", "x"), "no-such-machine.invalid");

    ASSERT_EQ(2u, statuses.size());
    EXPECT_EQ(FilePasteState::SENDING, statuses[0].state);
    EXPECT_EQ("sending 1 file(s) to no-such-machine.invalid with Taildrop", statuses[0].detail);
    EXPECT_EQ(FilePasteState::FAILED, statuses[1].state);
    EXPECT_EQ(0u, statuses[1].detail.find("Taildrop failed: ")) << statuses[1].detail;
    EXPECT_EQ("0123456789abcdef", statuses[1].request);
}

// sends a real file: run with --gtest_also_run_disabled_tests and
// LEAPDESK_TAILDROP_TARGET set to another machine's Tailscale address
TEST(MSWindowsFilePasteTests, DISABLED_taildrop_sendsAFile)
{
    const char* target = std::getenv("LEAPDESK_TAILDROP_TARGET");
    if (target == nullptr) {
        GTEST_SKIP() << "LEAPDESK_TAILDROP_TARGET is not set";
    }

    TempFolder folder;
    std::vector<FilePasteStatus> statuses =
        send_with_taildrop(folder.file(L"file.txt", "sent by MSWindowsFilePasteTests\n"), target);

    ASSERT_EQ(2u, statuses.size());
    EXPECT_EQ(FilePasteState::SENDING, statuses[0].state);
    EXPECT_EQ(FilePasteState::SENT, statuses[1].state) << statuses[1].detail;
}

namespace {

std::wstring ascii(const std::string& text)
{
    return std::wstring(text.begin(), text.end());
}

} // namespace

TEST(MSWindowsFilePasteTests, filePaste_findsTheFileThatArrived)
{
    TempFolder received;
    std::string prefix = FileClip::transfer_prefix("0123456789abcdef", 0);
    std::string name = prefix + "report.pdf";
    auto find = [&]() { return MSWindowsFilePaste::find_received(received.path(), prefix, name); };

    EXPECT_EQ(L"", find());

    // still being written, or not a file
    received.file(ascii(name) + L".partial", "x");
    received.folder(ascii(prefix) + L"folder");
    EXPECT_EQ(L"", find());

    // renamed on the way
    std::wstring renamed = received.file(ascii(prefix) + L"report (1).pdf", "x");
    EXPECT_EQ(renamed, find());

    // the name it was sent under comes first
    std::wstring exact = received.file(ascii(name), "x");
    EXPECT_EQ(exact, find());

    // another paste's files are left alone
    EXPECT_EQ(L"", MSWindowsFilePaste::find_received(
                       received.path(), FileClip::transfer_prefix("fedcba9876543210", 0),
                       FileClip::transfer_prefix("fedcba9876543210", 0) + "report.pdf"));
}

TEST(MSWindowsFilePasteTests, filePaste_cleansOnlyItsOwnStagingFolders)
{
    TempFolder outside;
    TempFolder staging;
    std::wstring old_paste = staging.folder(L"0123456789abcdef");
    staging.folder(L"0123456789abcdef\\1");
    staging.file(L"0123456789abcdef\\1\\report.pdf", "x");
    std::wstring current = staging.folder(L"fedcba9876543210");
    std::wstring other = staging.folder(L"not-a-paste");

    // a link out of the staging folder, named like a paste.  the service
    // cleans up as SYSTEM, so following it could delete anything
    std::wstring precious = outside.file(L"precious.txt", "keep me");
    std::wstring link = staging.path() + L"\\00000000000000aa";
    ASSERT_EQ(0, _wsystem((L"mklink /J \"" + link + L"\" \"" + outside.path() + L"\" >nul").c_str()));

    MSWindowsFilePaste::clean_staging(staging.path(), L"fedcba9876543210");

    EXPECT_FALSE(exists(old_paste));
    EXPECT_TRUE(exists(current));
    EXPECT_TRUE(exists(other));
    EXPECT_TRUE(exists(precious));
    RemoveDirectoryW(link.c_str());
}

TEST(MSWindowsFilePasteTests, filePaste_newRequestsAreValidAndDiffer)
{
    std::string first = MSWindowsFilePaste::new_request();
    std::string second = MSWindowsFilePaste::new_request();
    EXPECT_TRUE(is_valid_paste_request(first));
    EXPECT_EQ(16u, first.size());
    EXPECT_NE(first, second);
}

namespace {

// the tests below put data on the clipboard.  a window station has a
// clipboard of its own, so they run only in one that isn't the desktop's
bool in_private_window_station()
{
    wchar_t name[256] = {};
    DWORD needed = 0;
    GetUserObjectInformationW(GetProcessWindowStation(), UOI_NAME, name, sizeof(name), &needed);
    return _wcsicmp(name, L"WinSta0") != 0;
}

bool open_clipboard()
{
    for (int attempt = 0; attempt < 100; ++attempt) {
        if (OpenClipboard(nullptr)) {
            return true;
        }
        Sleep(10);
    }
    return false;
}

void set_clipboard_text(const std::wstring& text)
{
    ASSERT_TRUE(open_clipboard());
    EmptyClipboard();
    SIZE_T size = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, size);
    std::memcpy(GlobalLock(data), text.c_str(), size);
    GlobalUnlock(data);
    SetClipboardData(CF_UNICODETEXT, data);
    CloseClipboard();
}

std::wstring clipboard_text()
{
    std::wstring text;
    if (open_clipboard()) {
        HANDLE data = GetClipboardData(CF_UNICODETEXT);
        if (data != nullptr) {
            text = static_cast<const wchar_t*>(GlobalLock(data));
            GlobalUnlock(data);
        }
        CloseClipboard();
    }
    return text;
}

// what an application that pastes files gets: their paths, or none
std::vector<std::wstring> paste_files()
{
    std::vector<std::wstring> paths;
    if (open_clipboard()) {
        HANDLE data = GetClipboardData(CF_HDROP);
        if (data != nullptr) {
            paths = MSWindowsClipboardFilesConverter::get_paths(data);
        }
        CloseClipboard();
    }
    return paths;
}

FileClip two_files()
{
    FileClip clip;
    clip.files.push_back({"report.pdf", 5, 1});
    clip.files.push_back({"notes.txt", 3, 2});
    clip.id = FileClip::make_id({"C:\\a\\report.pdf", "C:\\a\\notes.txt"}, clip.files);
    return clip;
}

Clipboard files_clipboard(const FileClip& clip, const std::string& text)
{
    Clipboard clipboard;
    clipboard.open(0);
    clipboard.clear();
    clipboard.add(IClipboard::kText, text);
    clipboard.add(IClipboard::kFiles, clip.marshall());
    clipboard.close();
    return clipboard;
}

} // namespace

class MSWindowsFilePasteClipboardTests : public ::testing::Test {
protected:
    void SetUp() override
    {
        if (!in_private_window_station()) {
            GTEST_SKIP() << "puts data on the clipboard; run in a window station of its own";
        }
        ASSERT_TRUE(open_clipboard());
        EmptyClipboard();
        CloseClipboard();
    }

    void TearDown() override
    {
        for (auto& thread : threads_) {
            thread.join();
        }
        paste_.reset();
    }

    // settings for a paste whose files come from source_, which plays the
    // screen that copied them.  it answers on a thread of its own, like the
    // network would
    MSWindowsFilePaste::Settings settings()
    {
        MSWindowsFilePaste::Settings settings;
        settings.request = [this](const FilePasteRequest& request) {
            std::lock_guard<std::mutex> lock(mutex_);
            requests_.push_back(request);
            if (source_) {
                threads_.emplace_back([this, request]() { source_(request); });
            }
        };
        settings.own_address = []() { return std::string("100.64.0.9"); };
        settings.received_folder = [this]() { return received_.path(); };
        settings.staging_folder = staging_.path() + L"\\Pasted";
        settings.answer_timeout = std::chrono::seconds(5);
        settings.arrival_timeout = std::chrono::milliseconds(500);
        return settings;
    }

    void start(MSWindowsFilePaste::Settings settings)
    {
        paste_ = std::make_unique<MSWindowsFilePaste>(std::move(settings));
    }

    // a source that sends the files of \p clip with \p contents
    void send_files(const FileClip& clip, const std::vector<std::string>& contents)
    {
        source_ = [this, clip, contents](const FilePasteRequest& request) {
            paste_->status({request.request, FilePasteState::SENDING, "sending"});
            for (std::size_t i = 0; i < clip.files.size(); ++i) {
                received_.file(ascii(clip.transfer_name(request.request, i)), contents[i]);
            }
            paste_->status({request.request, FilePasteState::SENT, ""});
        };
    }

    // waits until the paste thread has put the offer on the clipboard
    bool offered()
    {
        for (int i = 0; i < 300; ++i) {
            if (IsClipboardFormatAvailable(CF_HDROP)) {
                return true;
            }
            Sleep(10);
        }
        return false;
    }

    std::vector<FilePasteRequest> requests()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return requests_;
    }

    TempFolder received_;
    TempFolder staging_;
    std::function<void(const FilePasteRequest&)> source_;
    std::unique_ptr<MSWindowsFilePaste> paste_;

private:
    std::mutex mutex_;
    std::vector<FilePasteRequest> requests_;
    std::vector<std::thread> threads_;
};

TEST_F(MSWindowsFilePasteClipboardTests, pastesFilesCopiedOnAnotherScreen)
{
    FileClip clip = two_files();
    send_files(clip, {"12345", "abc"});
    start(settings());

    ASSERT_TRUE(paste_->offer(files_clipboard(clip, "hello")));
    ASSERT_TRUE(offered());

    // the other formats are there at once, and nothing is fetched for them
    EXPECT_EQ(L"hello", clipboard_text());
    EXPECT_TRUE(requests().empty());

    std::vector<std::wstring> paths = paste_files();

    ASSERT_EQ(1u, requests().size());
    FilePasteRequest request = requests()[0];
    EXPECT_TRUE(is_valid_paste_request(request.request));
    EXPECT_EQ(clip.id, request.file_id);
    EXPECT_EQ("100.64.0.9", request.address);

    // under their own names, moved out of the folder they arrived in
    std::wstring folder = staging_.path() + L"\\Pasted\\" + ascii(request.request);
    ASSERT_EQ((std::vector<std::wstring>{folder + L"\\1\\report.pdf", folder + L"\\2\\notes.txt"}),
              paths);
    EXPECT_EQ("12345", read(paths[0]));
    EXPECT_EQ("abc", read(paths[1]));
    EXPECT_FALSE(exists(received_.path() + L"\\" + ascii(clip.transfer_name(request.request, 0))));

    // pasting again, or being offered the same files again as the server sets
    // its other clipboard, uses what was fetched
    ASSERT_TRUE(paste_->offer(files_clipboard(clip, "hello")));
    Sleep(200);
    EXPECT_EQ(paths, paste_files());
    EXPECT_EQ(1u, requests().size());
}

TEST_F(MSWindowsFilePasteClipboardTests, replacingTheClipboardCleansUp)
{
    FileClip clip = two_files();
    send_files(clip, {"12345", "abc"});
    start(settings());
    ASSERT_TRUE(paste_->offer(files_clipboard(clip, "hello")));
    ASSERT_TRUE(offered());
    std::vector<std::wstring> paths = paste_files();
    ASSERT_EQ(2u, paths.size());

    // the user copies something else on this machine
    set_clipboard_text(L"newer");

    EXPECT_FALSE(exists(paths[0]));
    EXPECT_FALSE(IsClipboardFormatAvailable(CF_HDROP));
}

TEST_F(MSWindowsFilePasteClipboardTests, failedSendGivesTheApplicationNothing)
{
    source_ = [this](const FilePasteRequest& request) {
        paste_->status({request.request, FilePasteState::FAILED,
                        "the files are no longer on the clipboard"});
    };
    start(settings());
    ASSERT_TRUE(paste_->offer(files_clipboard(two_files(), "hello")));
    ASSERT_TRUE(offered());

    EXPECT_TRUE(paste_files().empty());
    EXPECT_EQ(1u, requests().size());
}

TEST_F(MSWindowsFilePasteClipboardTests, unansweredPasteGivesUp)
{
    MSWindowsFilePaste::Settings quick = settings();
    quick.answer_timeout = std::chrono::milliseconds(300);
    start(std::move(quick));
    ASSERT_TRUE(paste_->offer(files_clipboard(two_files(), "hello")));
    ASSERT_TRUE(offered());

    auto started = std::chrono::steady_clock::now();
    EXPECT_TRUE(paste_files().empty());
    EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::seconds(3));
}

TEST_F(MSWindowsFilePasteClipboardTests, filesThatNeverArriveGiveNothing)
{
    source_ = [this](const FilePasteRequest& request) {
        paste_->status({request.request, FilePasteState::SENT, ""});
    };
    start(settings());
    ASSERT_TRUE(paste_->offer(files_clipboard(two_files(), "hello")));
    ASSERT_TRUE(offered());

    EXPECT_TRUE(paste_files().empty());
}

TEST_F(MSWindowsFilePasteClipboardTests, disconnectedPasteFailsAtOnce)
{
    start(settings());
    paste_->set_connected(false);
    ASSERT_TRUE(paste_->offer(files_clipboard(two_files(), "hello")));
    ASSERT_TRUE(offered());

    EXPECT_TRUE(paste_files().empty());
    EXPECT_TRUE(requests().empty());
}

TEST_F(MSWindowsFilePasteClipboardTests, newerClipboardWinsOverAnOfferOnItsWay)
{
    start(settings());
    for (int i = 0; i < 20; ++i) {
        // what the screen does when a text clipboard follows the files at once
        ASSERT_TRUE(paste_->offer(files_clipboard(two_files(), "files")));
        paste_->supersede();
        set_clipboard_text(L"text " + std::to_wstring(i));

        Sleep(20);
        EXPECT_FALSE(IsClipboardFormatAvailable(CF_HDROP)) << i;
        EXPECT_EQ(L"text " + std::to_wstring(i), clipboard_text()) << i;
    }
}

TEST_F(MSWindowsFilePasteClipboardTests, shellPastesIntoAFolder)
{
    FileClip clip = two_files();
    send_files(clip, {"12345", "abc"});
    start(settings());
    ASSERT_TRUE(paste_->offer(files_clipboard(clip, "hello")));
    ASSERT_TRUE(offered());

    // what Explorer does for Paste on a folder: the shell reads the clipboard
    // through OLE and copies the files with its own copy engine
    TempFolder destination;
    ASSERT_TRUE(SUCCEEDED(OleInitialize(nullptr)));
    PIDLIST_ABSOLUTE folder = nullptr;
    ASSERT_EQ(S_OK, SHParseDisplayName(destination.path().c_str(), nullptr, &folder, 0, nullptr));
    IShellFolder* parent = nullptr;
    PCUITEMID_CHILD child = nullptr;
    ASSERT_EQ(S_OK, SHBindToParent(folder, IID_PPV_ARGS(&parent), &child));
    IContextMenu* menu = nullptr;
    ASSERT_EQ(S_OK, parent->GetUIObjectOf(nullptr, 1, &child, IID_IContextMenu, nullptr,
                                          reinterpret_cast<void**>(&menu)));
    CMINVOKECOMMANDINFO invoke{};
    invoke.cbSize = sizeof(invoke);
    invoke.fMask = CMIC_MASK_FLAG_NO_UI;
    invoke.lpVerb = "paste";
    invoke.nShow = SW_HIDE;
    EXPECT_EQ(S_OK, menu->InvokeCommand(&invoke));

    // the copy may finish on a thread of its own, reporting to this one
    std::wstring report = destination.path() + L"\\report.pdf";
    std::wstring notes = destination.path() + L"\\notes.txt";
    for (int i = 0; i < 1000 && !(exists(report) && exists(notes)); ++i) {
        MSG message;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            DispatchMessageW(&message);
        }
        Sleep(10);
    }
    Sleep(200);
    EXPECT_EQ("12345", read(report));
    EXPECT_EQ("abc", read(notes));
    EXPECT_EQ(1u, requests().size());

    menu->Release();
    parent->Release();
    CoTaskMemFree(folder);
    OleUninitialize();
}

TEST_F(MSWindowsFilePasteClipboardTests, rejectsClipboardWithoutFiles)
{
    start(settings());
    Clipboard text;
    text.open(0);
    text.clear();
    text.add(IClipboard::kText, "just text");
    text.close();
    EXPECT_FALSE(paste_->offer(text));
}

} // namespace inputleap
