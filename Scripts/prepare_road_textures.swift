import CoreGraphics
import Foundation
import ImageIO
import UniformTypeIdentifiers

enum TextureError: Error {
    case cannotRead(String)
    case incompatibleDimensions
    case cannotCreateOutput
}

func loadRGBA(_ path: String) throws -> (pixels: [UInt8], width: Int, height: Int) {
    let url = URL(fileURLWithPath: path) as CFURL
    guard let source = CGImageSourceCreateWithURL(url, nil),
          let image = CGImageSourceCreateImageAtIndex(source, 0, nil) else {
        throw TextureError.cannotRead(path)
    }

    let width = image.width
    let height = image.height
    var pixels = [UInt8](repeating: 0, count: width * height * 4)
    guard let context = CGContext(
        data: &pixels,
        width: width,
        height: height,
        bitsPerComponent: 8,
        bytesPerRow: width * 4,
        space: CGColorSpaceCreateDeviceRGB(),
        bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue
    ) else {
        throw TextureError.cannotRead(path)
    }
    context.draw(image, in: CGRect(x: 0, y: 0, width: width, height: height))
    return (pixels, width, height)
}

guard CommandLine.arguments.count == 5 else {
    fputs("Usage: prepare_road_textures AO.jpg Roughness.jpg Metallic.jpg Output.png\n", stderr)
    exit(2)
}

let ao = try loadRGBA(CommandLine.arguments[1])
let roughness = try loadRGBA(CommandLine.arguments[2])
let metallic = try loadRGBA(CommandLine.arguments[3])
guard ao.width == roughness.width, ao.width == metallic.width,
      ao.height == roughness.height, ao.height == metallic.height else {
    throw TextureError.incompatibleDimensions
}

var orm = [UInt8](repeating: 255, count: ao.width * ao.height * 4)
for pixel in 0..<(ao.width * ao.height) {
    let offset = pixel * 4
    orm[offset] = ao.pixels[offset]
    orm[offset + 1] = roughness.pixels[offset]
    orm[offset + 2] = metallic.pixels[offset]
}

guard let provider = CGDataProvider(data: Data(orm) as CFData),
      let image = CGImage(
        width: ao.width,
        height: ao.height,
        bitsPerComponent: 8,
        bitsPerPixel: 32,
        bytesPerRow: ao.width * 4,
        space: CGColorSpaceCreateDeviceRGB(),
        bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.premultipliedLast.rawValue),
        provider: provider,
        decode: nil,
        shouldInterpolate: false,
        intent: .defaultIntent
      ) else {
    throw TextureError.cannotCreateOutput
}

let outputURL = URL(fileURLWithPath: CommandLine.arguments[4])
try FileManager.default.createDirectory(
    at: outputURL.deletingLastPathComponent(),
    withIntermediateDirectories: true
)
guard let destination = CGImageDestinationCreateWithURL(
    outputURL as CFURL,
    UTType.png.identifier as CFString,
    1,
    nil
) else {
    throw TextureError.cannotCreateOutput
}
CGImageDestinationAddImage(destination, image, nil)
guard CGImageDestinationFinalize(destination) else {
    throw TextureError.cannotCreateOutput
}

print("Created \(outputURL.path) (\(ao.width)x\(ao.height), R=AO G=Roughness B=Metallic)")
