# ビルビル / BILL BILL

Win32版と、ブラウザ版（VS CPU・Random Match・Room Match）を同じリポジトリで管理します。ゲームルールのC++実装は `Game.*` と `CpuPlayer.*` にあり、Win32 UIとWeb UIから分離されています。

## 必要環境

- Node.js 20以降
- WebAssembly版Coreを生成する場合: Emscripten SDK、CMake 3.20以降
- Win32版: Visual Studio 2022以降（Desktop development with C++）

## WebクライアントとWebSocketサーバー

```powershell
cd web
npm install
npm run dev
```

ブラウザで `http://localhost:8080` を開きます。`file://` で直接HTMLを開かないでください。サーバーは静的ファイルと `/ws` WebSocketを同じポートで提供します。

## Emscripten / WebAssemblyビルド

Emscripten SDKを有効化したターミナルで実行します。

```powershell
emcmake cmake -S . -B build-web
cmake --build build-web --config Release
```

`web/client/wasm/billbill.js` と `billbill.wasm` が生成されます。`WebBridge.cpp`は既存C++のGameとCPUへ、ブラウザから呼べる小さなC APIを公開します。

## ローカルテスト

サーバー起動中に別ターミナルで実行します。

```powershell
cd web
npm test
node --test server/integration.test.js
```

オンライン画面を手動確認する場合、通常ウィンドウとシークレットウィンドウで `http://localhost:8080` を開きます。

- Room Match: 片方でCREATE ROOM、もう片方で6桁コードをJOIN ROOMへ入力
- Random Match: 両方でFIND MATCHを押す
- Player Name保存: SETTINGSで名前を変更して再読み込み
- 音量保存: 音量とMuteを変更して再読み込み

## 設定

Web版はlocalStorageの `billbill.settings.v1` にPlayer Name、Master Volume、Muteだけを保存します。CPU難易度、Room Code、対戦状態は保存しません。

`web/client/config.js`で制作者名、Feedback URL、開発／本番WebSocket URLを一元管理します。本番ではHTTPS配信し、同一オリジンの `wss://<host>/ws` を使用する構成です。

## サーバー設計

サーバーはSession IDで接続を識別し、表示名をIDとして使いません。Random Queue、Room、Match、山札、手札、入札、結果をメモリ上で管理します。入札は所有カード・重複提出・Match参加者を検証し、両者確定まで相手の札を公開しません。初期版では再起動時に待機部屋と試合は失われ、切断後の再接続は行いません。

## 本番公開前の設定

- `web/client/config.js` の本番WebSocket接続先
- Node.js WebSocketサーバーを常時稼働できるホスティング先
- HTTPS / WSS、オリジン制限、レート制限、ログ監視
- 必要に応じて複数サーバー間でRoom／Match状態を共有するRedis等
