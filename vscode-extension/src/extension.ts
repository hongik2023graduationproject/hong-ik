import * as vscode from 'vscode';
import { LanguageClient, LanguageClientOptions, ServerOptions } from 'vscode-languageclient/node';

let client: LanguageClient | undefined;

export function activate(_context: vscode.ExtensionContext): void {
    const configured = vscode.workspace.getConfiguration('hongik').get<string>('lsp.path');
    const command = configured && configured.length > 0 ? configured : 'hongik-lsp';

    const serverOptions: ServerOptions = { command, args: [] };
    const clientOptions: LanguageClientOptions = {
        documentSelector: [{ language: 'hongik' }],
    };

    client = new LanguageClient('hongik-lsp', 'Hong-Ik Language Server', serverOptions, clientOptions);
    client.start().catch((err: unknown) => {
        void vscode.window.showWarningMessage(
            `hongik-lsp 시작 실패: ${String(err)} — 'hongik.lsp.path' 설정을 확인하세요. (하이라이팅은 계속 동작합니다)`
        );
    });
}

export function deactivate(): Thenable<void> | undefined {
    return client?.stop();
}
