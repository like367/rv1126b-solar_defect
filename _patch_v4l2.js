const fs = require('fs');
let code = fs.readFileSync('solar_defect.c', 'utf8');

// 1. Replace V4L2 init block
const v4l2Init = `    /* V4L2 */
    g_v4l2_fd=open(DEV,O_RDWR);
    struct v4l2_format fmt={0};
    fmt.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.pixelformat=V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.width=SRC_W; fmt.fmt.pix.height=SRC_H;
    fmt.fmt.pix.field=V4L2_FIELD_NONE;
    xioctl(g_v4l2_fd,VIDIOC_S_FMT,&fmt);
    printf("V4L2 fmt: %dx%d MJPEG\\n", fmt.fmt.pix.width, fmt.fmt.pix.height);

    struct v4l2_requestbuffers req={0};
    req.count=BUF_CNT; req.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory=V4L2_MEMORY_MMAP;
    xioctl(g_v4l2_fd,VIDIOC_REQBUFS,&req);
    for(int i=0;i<BUF_CNT;i++){
        struct v4l2_buffer buf={0};
        buf.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory=V4L2_MEMORY_MMAP; buf.index=i;
        xioctl(g_v4l2_fd,VIDIOC_QUERYBUF,&buf);
        g_bufs[i].length=buf.length;
        g_bufs[i].start=mmap(NULL,buf.length,PROT_READ|PROT_WRITE,
                             MAP_SHARED,g_v4l2_fd,buf.m.offset);
        xioctl(g_v4l2_fd,VIDIOC_QBUF,&buf);
    }
    enum v4l2_buf_type type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    xioctl(g_v4l2_fd,VIDIOC_STREAMON,&type);
    /* discard first 5 frames */
    for(int i=0;i<5;i++){
        struct v4l2_buffer buf={0};
        buf.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory=V4L2_MEMORY_MMAP;
        fd_set fds; struct timeval tv={2,0};
        FD_ZERO(&fds); FD_SET(g_v4l2_fd,&fds);
        select(g_v4l2_fd+1,&fds,NULL,NULL,&tv);
        xioctl(g_v4l2_fd,VIDIOC_DQBUF,&buf);
        xioctl(g_v4l2_fd,VIDIOC_QBUF,&buf);
    }
    printf("V4L2 OK\\n");`;

// Find "/* V4L2 Multiplanar */" ... "printf("V4L2 OK\n");"
const startMark = '/* V4L2 Multiplanar */';
const endMark = 'printf("V4L2 OK\\n");';
let start = code.indexOf(startMark);
let end = code.indexOf(endMark, start);
if (start < 0 || end < 0) {
    console.error('MARKERS NOT FOUND in V4L2 init. start=', start, ' end=', end);
    process.exit(1);
}
end += endMark.length;
code = code.substring(0, start) + v4l2Init + code.substring(end);

// 2. Replace capture_thread DQBUF/QBUF block (multiplanar → single, add JPEG decode)
const capThread = `    while(g_running){
        struct v4l2_buffer buf={0};
        buf.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory=V4L2_MEMORY_MMAP;
        fd_set fds; struct timeval tv={2,0};
        FD_ZERO(&fds); FD_SET(g_v4l2_fd,&fds);
        int sr_sel = select(g_v4l2_fd+1,&fds,NULL,NULL,&tv);
        if(sr_sel <= 0) {
            err_cnt++;
            if(err_cnt > 30) {
                fprintf(stderr,"[CAP] stream timeout, resetting...\\n");
                xioctl(g_v4l2_fd,VIDIOC_STREAMOFF,&buf.type);
                usleep(100000);
                xioctl(g_v4l2_fd,VIDIOC_STREAMON,&buf.type);
                err_cnt = 0;
            }
            continue;
        }
        if(xioctl(g_v4l2_fd,VIDIOC_DQBUF,&buf)<0) {
            err_cnt++;
            if(err_cnt > 5) {
                fprintf(stderr,"[CAP] DQBUF retry #%d\\n", err_cnt);
                usleep(50000);
            }
            continue;
        }
        err_cnt = 0;

        /* JPEG decode: MJPEG → RGB888 (libjpeg) */
        {
            struct jpeg_decompress_struct cinfo;
            struct jpeg_error_mgr jerr;
            cinfo.err = jpeg_std_error(&jerr);
            jpeg_create_decompress(&cinfo);
            jpeg_mem_src(&cinfo, g_bufs[buf.index].start, buf.bytesused);
            jpeg_read_header(&cinfo, TRUE);
            cinfo.out_color_space = JCS_RGB;
            jpeg_start_decompress(&cinfo);
            unsigned char *row = malloc(cinfo.output_width * 3);
            unsigned char *dst = tmp;
            while (cinfo.output_scanline < cinfo.output_height) {
                JSAMPROW rp[1] = { row };
                jpeg_read_scanlines(&cinfo, rp, 1);
                memcpy(dst, row, cinfo.output_width * 3);
                dst += cinfo.output_width * 3;
            }
            free(row);
            jpeg_finish_decompress(&cinfo);
            jpeg_destroy_decompress(&cinfo);
        }
        xioctl(g_v4l2_fd,VIDIOC_QBUF,&buf);

        Frame f={0};
        f.rgb=tmp; f.valid=1;
        queue_push(&g_cap_queue,&f);
    }`;

// Find capture_thread function body
const capStart = 'while(g_running){';
const capEnd = '    free(tmp);';
let cs = code.indexOf(capStart);
let ce = code.indexOf(capEnd, cs);
if (cs < 0 || ce < 0) {
    console.error('CAPTURE MARKERS NOT FOUND. cs=', cs, ' ce=', ce);
    process.exit(1);
}
code = code.substring(0, cs) + capThread + '\n' + code.substring(ce);

// 3. Fix cleanup: munmap (reverted from free)
code = code.replace(/for\(int i=0;i<BUF_CNT;i\+\+\) free\(g_bufs\[i\]\.start\);/,
    'for(int i=0;i<BUF_CNT;i++) munmap(g_bufs[i].start,g_bufs[i].length);');

fs.writeFileSync('solar_defect.c', code, 'utf8');
console.log('Patched successfully');
