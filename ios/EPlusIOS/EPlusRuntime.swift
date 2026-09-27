import Foundation
import SwiftUI
import SceneKit

struct EPlusEntity: Identifiable {
    let id = UUID()
    var name: String
    var type: String = "cube"
    var x: Float = 0
    var y: Float = 0
    var z: Float = 0
    var scale: Float = 1
    var health: Float = 100
    var speed: Float = 5
    var controllable = false
}

struct EPlusUIElement: Identifiable {
    let id = UUID()
    var name: String
    var type: String
    var text: String
    var x: CGFloat = 0
    var y: CGFloat = 0
    var width: CGFloat = 120
    var height: CGFloat = 40
}

struct EPlusScene {
    var title = "E#+"
    var width = 390
    var height = 844
    var background = Color(red: 0.04, green: 0.06, blue: 0.12)
    var canvas = false
    var entities: [EPlusEntity] = []
    var ui: [EPlusUIElement] = []
}

@MainActor
final class EPlusRuntime: ObservableObject {
    @Published var output = ""
    @Published var scene = EPlusScene()
    @Published var variables: [String: String] = [:]

    func run(_ source: String) {
        output = ""
        scene = EPlusScene()
        variables = [:]

        let lines = source.components(separatedBy: .newlines)
        var i = 0

        while i < lines.count {
            let raw = lines[i].trimmingCharacters(in: .whitespacesAndNewlines)
            if raw.isEmpty { i += 1; continue }

            if raw.hasPrefix("print words ") {
                output += interpolate(quoted(raw)) + "\n"
            } else if raw.hasPrefix("set ") {
                parseSet(raw)
            } else if raw.hasPrefix("game ") {
                scene.title = quoted(raw)
            } else if raw.hasPrefix("window width ") {
                scene.width = Int(raw.dropFirst(13).trimmingCharacters(in: .whitespaces)) ?? scene.width
            } else if raw.hasPrefix("window height ") {
                scene.height = Int(raw.dropFirst(14).trimmingCharacters(in: .whitespaces)) ?? scene.height
            } else if raw.hasPrefix("window title ") {
                scene.title = quoted(raw)
            } else if raw == "2d canvas" {
                scene.canvas = true
            } else if raw.hasPrefix("2d background ") {
                scene.background = parseColor(quoted(raw))
            } else if raw.hasPrefix("2d text ") {
                let parts = quotedParts(raw)
                if parts.count >= 2 {
                    scene.ui.append(EPlusUIElement(name: parts[0], type: "text", text: interpolate(parts[1])))
                }
            } else if raw.hasPrefix("2d button ") {
                let parts = quotedParts(raw)
                if parts.count >= 2 {
                    scene.ui.append(EPlusUIElement(name: parts[0], type: "button", text: interpolate(parts[1])))
                }
            } else if raw.hasPrefix("2d position ") {
                applyPosition(raw)
            } else if raw.hasPrefix("2d size ") {
                applySize(raw)
            } else if raw.hasPrefix("player create ") {
                let name = quoted(raw)
                scene.entities.append(EPlusEntity(name: name, controllable: true))
            } else if raw.hasPrefix("enemy create ") {
                scene.entities.append(EPlusEntity(name: quoted(raw)))
            } else if raw.hasPrefix("player position ") {
                applyEntityPosition(raw, player: true)
            } else if raw.hasPrefix("player speed ") {
                applyEntityFloat(raw, key: "speed", player: true)
            } else if raw.hasPrefix("player health ") {
                applyEntityFloat(raw, key: "health", player: true)
            } else if raw.hasPrefix("enemy health ") {
                applyEntityFloat(raw, key: "health", player: false)
            } else if raw.hasPrefix("enemy speed ") {
                applyEntityFloat(raw, key: "speed", player: false)
            } else if raw.hasPrefix("load model ") {
                let parts = quotedParts(raw)
                if let last = scene.entities.indices.last, parts.count >= 1 {
                    scene.entities[last].type = parts[0]
                }
            } else if raw.hasPrefix("ask user ") {
                output += "Input is available from the editor in a future interactive prompt.\n"
            } else if raw.hasPrefix("repeat ") {
                let count = Int(raw.split(separator: " ").dropFirst().first ?? "0") ?? 0
                var body: [String] = []
                i += 1
                while i < lines.count && lines[i].trimmingCharacters(in: .whitespacesAndNewlines) != "end" {
                    body.append(lines[i])
                    i += 1
                }
                for _ in 0..<count { run(body.joined(separator: "\n")) }
            }
            i += 1
        }
    }

    private func parseSet(_ line: String) {
        let rest = String(line.dropFirst(4))
        guard let range = rest.range(of: " to ") else { return }
        let name = rest[..<range.lowerBound].trimmingCharacters(in: .whitespaces)
        let value = rest[range.upperBound...].trimmingCharacters(in: .whitespaces)
        variables[String(name)] = interpolate(value.hasPrefix(""") ? quoted(value) : String(value))
    }

    private func interpolate(_ value: String) -> String {
        var result = value
        for (key, value) in variables {
            result = result.replacingOccurrences(of: key, with: value)
        }
        return result
    }

    private func quoted(_ line: String) -> String {
        guard let first = line.firstIndex(of: """),
              let last = line.lastIndex(of: """), last > first else { return "" }
        return String(line[line.index(after: first)..<last])
    }

    private func quotedParts(_ line: String) -> [String] {
        var result: [String] = []
        var rest = line
        while let first = rest.firstIndex(of: """) {
            rest = String(rest[rest.index(after: first)...])
            guard let last = rest.firstIndex(of: """) else { break }
            result.append(String(rest[..<last]))
            rest = String(rest[rest.index(after: last)...])
        }
        return result
    }

    private func applyPosition(_ line: String) {
        let parts = line.split(separator: " ")
        guard parts.count >= 5, let x = Double(parts[3]), let y = Double(parts[4]) else { return }
        if let index = scene.ui.firstIndex(where: { $0.name == String(parts[2]) }) {
            scene.ui[index].x = CGFloat(x)
            scene.ui[index].y = CGFloat(y)
        }
    }

    private func applySize(_ line: String) {
        let parts = line.split(separator: " ")
        guard parts.count >= 5, let w = Double(parts[3]), let h = Double(parts[4]) else { return }
        if let index = scene.ui.firstIndex(where: { $0.name == String(parts[2]) }) {
            scene.ui[index].width = CGFloat(w)
            scene.ui[index].height = CGFloat(h)
        }
    }

    private func applyEntityPosition(_ line: String, player: Bool) {
        let p = line.split(separator: " ")
        guard p.count >= 5, let x = Float(p[2]), let y = Float(p[3]), let z = Float(p[4]) else { return }
        if let index = scene.entities.firstIndex(where: { player ? $0.controllable : !$0.controllable }) {
            scene.entities[index].x = x; scene.entities[index].y = y; scene.entities[index].z = z
        }
    }

    private func applyEntityFloat(_ line: String, key: String, player: Bool) {
        let p = line.split(separator: " ")
        guard let value = Float(p.last ?? "") else { return }
        if let index = scene.entities.firstIndex(where: { player ? $0.controllable : !$0.controllable }) {
            if key == "speed" { scene.entities[index].speed = value }
            if key == "health" { scene.entities[index].health = value }
        }
    }

    private func parseColor(_ value: String) -> Color {
        let hex = value.replacingOccurrences(of: "#", with: "")
        guard hex.count == 6, let n = UInt64(hex, radix: 16) else { return scene.background }
        return Color(
            red: Double((n >> 16) & 255) / 255,
            green: Double((n >> 8) & 255) / 255,
            blue: Double(n & 255) / 255
        )
    }
}

struct EPlusGameView: View {
    let scene: EPlusScene
    @State private var pressed: String?

    var body: some View {
        ZStack {
            scene.background.ignoresSafeArea()

            if scene.canvas {
                ForEach(scene.ui) { element in
                    if element.type == "text" {
                        Text(element.text)
                            .font(.system(size: 24, weight: .medium))
                            .foregroundStyle(.white)
                            .position(x: element.x + 80, y: element.y + 20)
                    } else {
                        Button(element.text) { pressed = element.name }
                            .buttonStyle(.borderedProminent)
                            .frame(width: element.width, height: element.height)
                            .position(x: element.x + element.width / 2, y: element.y + element.height / 2)
                    }
                }
            } else if !scene.entities.isEmpty {
                SceneView(scene: make3DScene(), options: [.allowsCameraControl])
                    .ignoresSafeArea()
            } else {
                Text(scene.title).font(.largeTitle).foregroundStyle(.white)
            }
        }
        .navigationTitle(scene.title)
    }

    private func make3DScene() -> SCNScene {
        let result = SCNScene()
        result.background.contents = UIColor.black
        let camera = SCNCamera()
        let cameraNode = SCNNode()
        cameraNode.camera = camera
        cameraNode.position = SCNVector3(0, 3, 10)
        result.rootNode.addChildNode(cameraNode)

        let light = SCNLight()
        light.type = .omni
        light.intensity = 1200
        let lightNode = SCNNode()
        lightNode.light = light
        lightNode.position = SCNVector3(4, 8, 6)
        result.rootNode.addChildNode(lightNode)

        for entity in scene.entities {
            let geometry: SCNGeometry = entity.type.lowercased().contains("sphere")
                ? SCNSphere(radius: CGFloat(entity.scale))
                : SCNBox(width: CGFloat(entity.scale), height: CGFloat(entity.scale), length: CGFloat(entity.scale), chamferRadius: 0)
            geometry.firstMaterial?.diffuse.contents = entity.controllable ? UIColor.systemBlue : UIColor.systemRed
            let node = SCNNode(geometry: geometry)
            node.position = SCNVector3(entity.x, entity.y, entity.z)
            result.rootNode.addChildNode(node)
        }
        return result
    }
}
