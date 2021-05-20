//
//  ProgressImageView.swift
//  ImageDownloader
//
//  Created by Saad on 5/19/21.
//

import UIKit

class ProgressImageView: UIImageView {
    private var progressView: UIProgressView!

    required init?(coder: NSCoder) {
        super.init(coder: coder)
        backgroundColor = UIColor.systemGroupedBackground
        isOpaque = false
        self.contentMode = .scaleAspectFill

        progressView = UIProgressView(progressViewStyle: .default)
        progressView.setProgress(0.0, animated: true)
        progressView.trackTintColor = UIColor.lightGray
        progressView.tintColor = UIColor.blue
        addSubview(progressView)

    }
    override func layoutSubviews() {
        super.layoutSubviews()
        progressView.center = CGPoint(x: bounds.width/2, y: bounds.height/2)
    }
    func finishLoading() {
        self.progressView.setProgress(1.0, animated: true)
        self.progressView.isHidden = true
    }

    func updateProgress(progress: Float) {
        self.progressView.setProgress(progress, animated: true)
    }
    var isLoading: Bool {
        get{
            return !progressView.isHidden
        }
    }

}
