import UIKit
import LatencyPlayer

final class ViewController: UIViewController, LatencyPlayerCallback {

    private let playerView = LatencyPlayerView()
    private let player = LatencyPlayerManager()

    private let rtmpURL = "rtmp://aliyunjp02.caiyunwang.net/xiaochou/141"
        + "?auth_key=1782553361-0-0-a1c430ba36349758563f81bb0a126ce5"

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .black

        playerView.frame = view.bounds
        playerView.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        view.addSubview(playerView)

        player.initialize()

        let config = LatencyPlayerConfig.build { c in
            c.url = self.rtmpURL
            c.bufferTime = 200
            c.autoReconnect = true
            c.reconnectInterval = 3
            c.lowLatency = true
        }
        player.setConfig(config)
        player.setSurface(playerView)
        player.setCallback(self)
        player.start()
    }

    override func viewDidDisappear(_ animated: Bool) {
        super.viewDidDisappear(animated)
        player.releasePlayer()
    }

    // MARK: - LatencyPlayerCallback

    func onStateChanged(_ state: LatencyPlayerState) {
        print("state: \(LatencyPlayerStateToString(state))")
    }

    func onError(_ errorCode: LatencyPlayerErrorCode, message: String) {
        print("error: \(errorCode) \(message)")
    }

    func onBuffering(_ percent: Int32) {
        print("buffering: \(percent)%")
    }

    func onConnected() {
        print("connected")
    }

    func onDisconnected() {
        print("disconnected")
    }

    func onFirstFrameRendered() {
        print("first frame rendered")
    }

    func onDownloadSpeed(_ bytesPerSec: Int64) {
        // print("download: \(bytesPerSec) B/s")
    }

    func onReconnecting(_ attemptCount: Int32) {
        print("reconnecting: \(attemptCount)")
    }

    func onVideoSizeChanged(_ width: Int32, height: Int32) {
        print("video size: \(width)x\(height)")
    }

    func onPlaybackCompleted() {
        print("playback completed")
    }
}
