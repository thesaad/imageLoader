//
//  ViewController.swift
//  ImageDownloader
//
//  Created by Saad on 5/19/21.
//

import UIKit

class ViewController: UIViewController {

    @IBOutlet weak var imagView1: ProgressImageView!
    @IBOutlet weak var imagView2: ProgressImageView!
    @IBOutlet weak var imagView3: ProgressImageView!

    @IBOutlet weak var uploadProgressBar: UIProgressView!
    @IBOutlet weak var uploadFileLabel: UILabel!
    @IBOutlet weak var uploadButton: UIButton!

    //let imageUrls = ["https://farm6.staticflickr.com/5720/22076039308_4e2fc21c5f_o.jpg",
//                         "https://www.satsignal.eu/wxsat/Meteosat7-full-scan.jpg",
//                         "https://i.pinimg.com/originals/19/e9/58/19e9581dbdc756a2dbbb38ae39a3419c.jpg"]//larger files

    let imageUrls = ["https://asia.olympus-imaging.com/content/000107506.jpg",
                         "https://13pk8g3890ly2a4dv72jipdq-wpengine.netdna-ssl.com/wp-content/uploads/2016/01/Nikon-1-V3-sample-photo.jpg",
                         "https://mars.nasa.gov/system/news_items/main_images/8936_First_selfie_animation_1200.jpg"]//smaller files

    let bufferLength = 1024
    var currentImageUpladeIndex = -1
    var inputStream: InputStream!
    var outputStream: OutputStream!

    let imageDownloadQueue = DispatchQueue(
        label: "com.imageDownload.queue",
        attributes: .concurrent
    )
    let imageUploadQueue = DispatchQueue(
        label: "com.imageUpload.queue"
    )

    override func viewDidLoad() {
        super.viewDidLoad()
        // Do any additional setup after loading the view.
        
        startDownloadingImages()
    }

    func startDownloadingImages() {
        let imageViews:[ProgressImageView] = [imagView1, imagView2, imagView3]
        for i in 0..<imageUrls.count {

            if let url = URL(string:imageUrls[i])
            {
                imageDownloadQueue.async {
                    ImageDownloadManager().loadImage(url: url) { (image, error) in
                        DispatchQueue.main.async {
                            imageViews[i].image = image
                            imageViews[i].finishLoading()
                            if !imageViews[0].isLoading && !imageViews[0].isLoading && !imageViews[0].isLoading {
                                self.uploadButton.isHidden = false
                            }
                        }

                    } progress: { (progress) in
                        DispatchQueue.main.async {
                            imageViews[i].updateProgress(progress: progress)
                        }
                    }
                }


            }

        }

    }
    @IBAction func uploadeImagesTapped(sender:UIButton)
    {
        self.uploadFileLabel.isHidden = false
        self.uploadProgressBar.isHidden = false
        setupServerConnection()
    }
    func setupServerConnection() {

      var readStream: Unmanaged<CFReadStream>?
      var writeStream: Unmanaged<CFWriteStream>?

      
      CFStreamCreatePairWithSocketToHost(kCFAllocatorDefault,
                                         "localhost" as CFString,
                                         2223,
                                         &readStream,
                                         &writeStream)

      inputStream = readStream!.takeRetainedValue()
      outputStream = writeStream!.takeRetainedValue()

      inputStream.delegate = self

      inputStream.schedule(in: .current, forMode: .common)
      outputStream.schedule(in: .current, forMode: .common)

      inputStream.open()
      outputStream.open()
    }
    func updateUploadeFile(progress:Float)
    {
        DispatchQueue.main.async {
            self.uploadFileLabel.text = "Uploading Image \(self.currentImageUpladeIndex + 1)"
            self.uploadProgressBar.setProgress(progress, animated: true)
        }

    }
}

extension ViewController: StreamDelegate
{

    func stream(_ aStream: Stream, handle eventCode: Stream.Event) {
        switch eventCode {
        case .hasBytesAvailable:
            print("new message received")
            readAvailableBytes(stream: aStream as! InputStream)
        case .endEncountered:
            print("new message received with end flag")
        case .errorOccurred:
            print("error occurred")
        case .hasSpaceAvailable:
            print("has space available")
        default:
            print("some other event...")
        }
    }

    private func readAvailableBytes(stream: InputStream) {
        let buffer = UnsafeMutablePointer<UInt8>.allocate(capacity: bufferLength)
        while stream.hasBytesAvailable {
            let numberOfBytesRead = inputStream.read(buffer, maxLength: bufferLength)

            if numberOfBytesRead < 0, let error = stream.streamError {
                print(error)
                break
            }
            let dataRecvd = Data(bytes: buffer, count: numberOfBytesRead)
            if let stringReceived = String(data: dataRecvd, encoding: .utf8)
            {
                print("Messeage From Server: \(stringReceived)")
//                imageUploadQueue.async {
                    if stringReceived.contains("01-Hello from server") {
                        //initial messsage. Start Handshake for sending first file
                        self.currentImageUpladeIndex = 0
                        self.triggerFileUploadHandShake()
                    }
                    else if stringReceived.contains("200-FILEHANDSHAKE OK") {
                        //send file at current index
                        self.updateUploadeFile(progress: 0.0)
                        self.triggerFileUpload()
                        
                    }
                    else if stringReceived.contains("200-FILERECEIVE OK") {
                        //start next file
                        if self.currentImageUpladeIndex < self.imageUrls.count {
                            self.currentImageUpladeIndex += 1
                            self.triggerFileUploadHandShake()

                        }
                        else{
                            //finished. Close every socket files etc.
                            print("All files are sent")
                        }
                    }
//                }
            }
        }
    }
    func triggerFileUploadHandShake() {

        let fileSize = ImageDownloadManager.fileSize(imageUrl: imageUrls[currentImageUpladeIndex])
        if fileSize > 0 {
            if let url = URL(string: imageUrls[currentImageUpladeIndex]), let data = "SEND \(url.lastPathComponent) \(fileSize))".data(using: .utf8)
            {
                data.withUnsafeBytes {
                    guard let pointer = $0.baseAddress?.assumingMemoryBound(to: UInt8.self) else {
                        print("Error sending Comamnd File Handshake")
                        return
                    }
                    outputStream.write(pointer, maxLength: data.count)
                }
            }
        }

    }
    func triggerFileUpload() {

        do{
            if let url = URL(string: imageUrls[currentImageUpladeIndex])
            {
                let data =  try Data(contentsOf: url)
                let dataLen = data.count
                let chunkSize = bufferLength
                let fullChunks = Int(dataLen / chunkSize)
                let totalChunks = fullChunks + (dataLen % chunkSize != 0 ? 1 : 0)

                for chunkCounter in 0..<totalChunks {
                    let chunkBase = chunkCounter * chunkSize
                    var diff = chunkSize
                    if(chunkCounter == totalChunks - 1) {
                        diff = dataLen - chunkBase
                    }

                    let range:Range<Data.Index> = (chunkBase..<(chunkBase + diff))
                    let chunk = data.subdata(in: range)

                    chunk.withUnsafeBytes {
                        guard let pointer = $0.baseAddress?.assumingMemoryBound(to: UInt8.self) else {
                            print("Error sending file chunk")
                            return
                        }
                        outputStream.write(pointer, maxLength: data.count)
                        print("Sent \(chunk.count) bytes. ")
                        let uploadedPercentage: Float = (Float((data.count - diff))/Float(data.count))
                        self.updateUploadeFile(progress: uploadedPercentage)
                        print("Uploaded percentage: \(uploadedPercentage)")
                    }
                }
                if let data = "EOF_FOUND_FILE_COMPLETED".data(using: .utf8)
                {
                    data.withUnsafeBytes {
                        guard let pointer = $0.baseAddress?.assumingMemoryBound(to: UInt8.self) else {
                            print("Error sending Comamnd File Finished")
                            return
                        }
                        outputStream.write(pointer, maxLength: data.count)
                    }
                }


            }
        }
        catch let e
        {
            print("Error Sending file: \(e.localizedDescription)")
        }
    }

}
