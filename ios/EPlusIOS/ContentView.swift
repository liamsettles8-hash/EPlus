import SwiftUI
import UniformTypeIdentifiers

struct ContentView: View {
    @StateObject private var runtime = EPlusRuntime()
    @State private var source = """
game "E#+ iOS"

window width 390
window height 844
window title "E#+ iOS"

2d canvas
2d background "#0B1020"

2d text "title" "E#+ iOS"
2d position "title" 30 40

2d text "welcome" "Native E#+ on iPhone"
2d position "welcome" 30 85

2d button "run" "Run E#+"
2d position "run" 30 140
2d size "run" 220 55
"""
    @State private var showingFile = false
    @State private var showingGame = false

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                HStack {
                    Button("Open") { showingFile = true }
                    Spacer()
                    Button("Run") {
                        runtime.run(source)
                        showingGame = true
                    }
                    .buttonStyle(.borderedProminent)
                }
                .padding()

                TextEditor(text: $source)
                    .font(.system(.body, design: .monospaced))
                    .scrollContentBackground(.hidden)
                    .background(Color.black.opacity(0.18))

                if !runtime.output.isEmpty {
                    ScrollView {
                        Text(runtime.output)
                            .font(.system(.footnote, design: .monospaced))
                            .frame(maxWidth: .infinity, alignment: .leading)
                            .padding()
                    }
                    .frame(maxHeight: 130)
                    .background(.black.opacity(0.15))
                }
            }
            .navigationTitle("E#+")
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Menu {
                        Button("New") { source = "" }
                        Button("Example: Desktop") { source = desktopExample }
                        Button("Example: 3D") { source = gameExample }
                    } label: {
                        Image(systemName: "ellipsis.circle")
                    }
                }
            }
            .fileImporter(
                isPresented: $showingFile,
                allowedContentTypes: [.plainText, UTType(filenameExtension: "eplus") ?? .plainText]
            ) { result in
                guard case .success(let url) = result else { return }
                if let text = try? String(contentsOf: url, encoding: .utf8) {
                    source = text
                }
            }
            .sheet(isPresented: $showingGame) {
                NavigationStack {
                    EPlusGameView(scene: runtime.scene)
                }
            }
        }
    }

    private var desktopExample: String {
        """
        game "E#+ Desktop"

        window width 390
        window height 844
        window title "E#+ Desktop"

        2d canvas
        2d background "#10131A"

        2d text "title" "E#+ OS"
        2d position "title" 30 35

        2d button "terminal" "Terminal"
        2d position "terminal" 30 100
        2d size "terminal" 220 55

        2d button "files" "File Manager"
        2d position "files" 30 170
        2d size "files" 220 55

        2d button "settings" "Settings"
        2d position "settings" 30 240
        2d size "settings" 220 55
        """
    }

    private var gameExample: String {
        """
        game "E#+ Test Arena"

        window width 390
        window height 844
        window title "E#+ Test Arena"

        camera first person

        player create "Player"
        player position 0 1 0
        player speed 7
        player health 100

        enemy create "Enemy"
        enemy health 50
        enemy speed 2

        load model "cube"
        """
    }
}
