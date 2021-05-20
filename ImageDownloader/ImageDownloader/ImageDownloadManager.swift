//
//  ImageDownloadManager.swift
//  ImageDownloader
//
//  Created by Saad on 5/19/21.
//

import UIKit

class ImageDownloadManager: NSObject {
    static let shared = ImageDownloadManager()
    private var observation: NSKeyValueObservation?
    var progressHandler: ((Float) -> ())?
    func loadImage(url: URL, completion: @escaping (UIImage?, Error?) -> Void, progress: @escaping (Float) -> Void) {
        // Compute a path to the URL in the cache
        self.progressHandler = progress
        let fileCachePath = FileManager.default.temporaryDirectory
            .appendingPathComponent(
                url.lastPathComponent,
                isDirectory: false
            )

        // If the image exists in the cache,
        // load the image from the cache and exit
        if let image = UIImage(contentsOfFile: fileCachePath.path)

        {
            completion(image, nil)
            return
        }

        // If the image does not exist in the cache,
        // download the image to the cache
        else
        {

            download(url: url, toFile: fileCachePath) { (error) in
                if let image = UIImage(contentsOfFile: fileCachePath.path)
                {
                    completion(image, error)
                }
                else
                {
                    completion(nil, error)
                }

            }
        }
    }
    func download(url: URL, toFile file: URL, completion: @escaping (Error?) -> Void) {
        // Download the remote URL to a file
        let task = URLSession.shared.downloadTask(with: url) {
            (tempURL, response, error) in
            // Early exit on error
            guard let tempURL = tempURL else {
                completion(error)
                return
            }

            do {
                // Remove any existing document at file
                if FileManager.default.fileExists(atPath: file.path) {
                    try FileManager.default.removeItem(at: file)
                }

                // Copy the tempURL to file
                try FileManager.default.copyItem(
                    at: tempURL,
                    to: file
                )

                completion(nil)
            }

            // Handle potential file system errors
            catch let fileError {
                completion(fileError)
            }
        }
        
        observation = task.progress.observe(\.fractionCompleted) { progress, _ in
            print("progress: \(task.taskIdentifier)", progress.fractionCompleted)
            self.progressHandler?(Float(progress.fractionCompleted))
        }
        // Start the download
        task.resume()

    }
    static func filePath(url:URL) -> URL
    {
        let fileCachePath = FileManager.default.temporaryDirectory
            .appendingPathComponent(
                url.lastPathComponent,
                isDirectory: false
            )
        return fileCachePath
    }
    static func fileSize(imageUrl:String) -> Int64
    {
        if let url = URL(string: imageUrl) {
            let path = filePath(url: url).path
            do{
                let attributes = try FileManager.default.attributesOfItem(atPath: path)
                guard let size = attributes[.size] as? Int64 else {
                    return 0
                }
                return size

            }
            catch let e
            {
                print("Error getting fileSize: \(e.localizedDescription)")
                return 0
            }
        }
        return 0

    }

}
