#include "stdafx.h"
#include "macros.h"

#undef UNIT_TEST_BATCH
#define UNIT_TEST_BATCH Vorb_IO_

#include <include/IO.h>
#include <include/graphics/ModelIO.h>
#include <include/io/IOManager.h>
#include <include/Vorb.h>
#include <include/Timing.h>

TEST(Path) {
    vpath path = ".";
    if (!path.isValid()) return false;

    printf("Empty:     %s\n", path.isNull() ? "True" : "False");
    printf("Valid:     %s\n", path.isValid() ? "True" : "False");
    printf("File:      %s\n", path.isFile() ? "True" : "False");
    printf("Directory: %s\n", path.isDirectory() ? "True" : "False");
    printf("Path:      %s\n", path.getCString());
    printf("Abs. Path: %s\n", path.asAbsolute().getCString());
    path.makeAbsolute();
    path--;
    printf("Parent:    %s\n", path.getCString());
 
    path = "C:";
    path--;
    if (!path.isNull()) return false;
    
    return true;
}

TEST(DirectoryEnum) {
    vpath path = ".";
    path.makeAbsolute();
    vdir dir;
    if (!path.asDirectory(&dir)) return false;

    dir.forEachEntry(createDelegate<const vpath&>([=] (Sender s, const vpath& e) {
        if (!e.isValid()) return;
        printf("Entry: %s\n", e.getCString());
    }));

    dir.forEachEntry([=](Sender s, const vpath& e) {
        if (!e.isValid()) return;
        printf("Entry: %s\n", e.getCString());
    });

    return true;
}

TEST(WriteTestTxt) {
    vpath path = "test/test.txt";
    vfile file;
    if (!path.asFile(&file)) return false;
    vfstream fs = file.create(false);
    if (!fs.isOpened()) return false;
    fs.write("Hello World\n");

    fs = file.open(false);
    char buf[100];
    fs.read(12, 1, buf);
    buf[12] = 0;
    if (strcmp(buf, "Hello World\n") != 0) return false;
    return true;
}

TEST(CheckSum) {
    vpath path = "data/checksums";
    vio::DirectoryEntries entries;
    vdir dir;
    path.asDirectory(&dir);
    dir.forEachEntry([] (Sender s, const vpath& path) {
        vfile file;
        if (!path.asFile(&file)) return;
        vio::SHA256Sum sum;
        file.computeSum(&sum);
        printf("\nSHA256 Checksum: <%s>\n", file.getPath().getCString());
        for (size_t i = 0; i < 8;) {
            printf("0x%08X ", sum[i++]);
            printf("0x%08X\n", sum[i++]);
        }
        printf("\n");
    });

    return true;
}

TEST(IOMDirs) {
    if (vorb::init(vorb::InitParam::IO) != vorb::InitParam::IO) return false;

    printf("Exec Dir: %s\n", vio::IOManager::getExecutableDirectory().getCString());
    printf("CWD Dir:  %s\n", vio::IOManager::getCurrentWorkingDirectory().getCString());

    return true;
}

TEST(ModelIO) {
    vpath path = "data/models/WaveOBJ";
    vio::DirectoryEntries entries;
    vdir dir;
    path.asDirectory(&dir);

    dir.forEachEntry([] (Sender s, const vpath& path) {
        PreciseTimer timer;

        vfile file;
        if (!path.asFile(&file)) return;

        vfstream fs = file.openReadOnly(false);
        vio::FileSeekOffset l = fs.length();
        cString data = new char[l + 1];
        l = (size_t)fs.read(l, 1, data);
        data[l] = 0;
        fs.close();

        vg::OBJMesh mesh;
        timer.start();
        vg::ModelIO::loadOBJ(data, mesh);
        f32 ms = timer.stop();
        delete[] data;

        printf("Model: %s\n", file.getPath().getCString());
        printf("Verts: %d\n", mesh.vertices.size());
        printf("Inds: %d\n", mesh.triangles.size() * 3);
        printf("Load Time (MS): %f\n", ms);
    });

    return true;
}

TEST(VRAW) {
    vpath path = "data/models/VRAW";
    vio::DirectoryEntries entries;
    vdir dir;
    path.asDirectory(&dir);

    dir.forEachEntry([] (Sender s, const vpath& path) {
        PreciseTimer timer;

        vfile file;
        if (!path.asFile(&file)) return;

        vfstream fs = file.openReadOnly(true);
        vio::FileSeekOffset l = fs.length();
        cString data = new char[l + 1];
        l = (size_t)fs.read(l, 1, data);
        fs.close();

        vg::MeshDataRaw mesh;
        timer.start();
        for (size_t i = 0; i < 100; i++) {
            vg::VertexDeclaration vdecl;
            size_t indexSize;
            mesh = vg::ModelIO::loadRAW(data, vdecl, indexSize);
            delete[] mesh.vertices;
            delete[] mesh.indices;
        }
        f32 ms = timer.stop();
        delete[] data;

        printf("Model: %s\n", file.getPath().getCString());
        printf("Verts: %d\n", mesh.vertexCount);
        printf("Inds: %d\n", mesh.indexCount);
        printf("Load Time (MS): %f\n", ms);
    });

    return true;
}

TEST(Animation) {
    vpath path = "data/animation/";
    vio::DirectoryEntries entries;
    vdir dir;
    path.asDirectory(&dir);

    dir.forEachEntry([] (Sender s, const vpath& path) {
        PreciseTimer timer;

        vfile file;
        if (!path.asFile(&file)) return;

        vfstream fs = file.openReadOnly(true);
        vio::FileSeekOffset l = fs.length();
        cString data = new char[l];
        l = (size_t)fs.read(l, 1, data);
        fs.close();

        vg::Skeleton skeleton;
        timer.start();
        skeleton = vg::ModelIO::loadAnim(data);
        f32 ms = timer.stop();
        delete[] data;

        printf("Animation: %s\n", file.getPath().getCString());
        printf("Bones:  %d\n", skeleton.numBones);
        printf("Frames: %d\n", skeleton.numFrames);
        printf("Load Time (MS): %f\n", ms);
    });

    return true;
}