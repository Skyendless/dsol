#include "sv/dsol/frame.h"

#include <benchmark/benchmark.h>
#include <gtest/gtest.h>

namespace sv::dsol {
namespace {

namespace bm = benchmark;

constexpr int kImageSize = 640;
constexpr int kCellSize = 16;
constexpr int kNumLevels = 4;

class FrameTest : public ::testing::Test {
 protected:
  void SetUp() override {
    image = MakeRandMat8U(kImageSize);
    MakeImagePyramid(image, kNumLevels, images);
    frame = Frame(images, Sophus::SE3d{});
    keyframe.SetFrame(frame);
  }

 public:
  cv::Mat image;
  ImagePyramid images;
  Frame frame;
  Keyframe keyframe;
};

TEST_F(FrameTest, TestMonoCtor) {
  EXPECT_FALSE(frame.empty());
  EXPECT_FALSE(frame.is_stereo());
  EXPECT_EQ(frame.levels(), kNumLevels);
  EXPECT_EQ(frame.cvsize().height, kImageSize);
  EXPECT_EQ(frame.cvsize().width, kImageSize);
}

TEST_F(FrameTest, TestStereoCtor) {
  Frame stereo_frame(images, images, {});
  EXPECT_FALSE(stereo_frame.empty());
  EXPECT_TRUE(stereo_frame.is_stereo());
}

TEST_F(FrameTest, TestSetFrameMono) {
  //
  EXPECT_FALSE(keyframe.is_stereo());
}

TEST_F(FrameTest, TestSetFrameStereo) {
  Frame stereo_frame(images, images, {});
  Keyframe kf;
  kf.SetFrame(stereo_frame);
  EXPECT_TRUE(kf.is_stereo());
}

TEST_F(FrameTest, TestKeyframeStatus) {
  EXPECT_TRUE(!keyframe.Ok());
  EXPECT_TRUE(!keyframe.Precomputed());
}

TEST(KeyframeTest, TestFixed) {
  using Eigen::Vector3d;

  Keyframe kf;
  EXPECT_EQ(kf.is_fixed(), false);

  Frame::Vector10d dx;
  dx.setZero();
  dx.segment<3>(3).setOnes();
  kf.UpdateState(dx);

  auto st = kf.GetFirstEstimate();
  EXPECT_EQ(st.T_w_cl.translation(), Vector3d::Ones());
  EXPECT_EQ(kf.Twc().translation(), Vector3d::Ones());

  kf.SetFixed();
  EXPECT_EQ(kf.is_fixed(), true);

  kf.UpdateState(dx);
  st = kf.GetFirstEstimate();
  EXPECT_EQ(st.T_w_cl.translation(), Vector3d::Ones());
  EXPECT_EQ(kf.Twc().translation(), Vector3d::Constant(1));

  kf.UpdateState(dx);
  st = kf.GetFirstEstimate();
  EXPECT_EQ(st.T_w_cl.translation(), Vector3d::Ones());
  EXPECT_EQ(kf.Twc().translation(), Vector3d::Constant(1));

  kf.UpdateLinearizationPoint();
  st = kf.GetFirstEstimate();
  EXPECT_EQ(st.T_w_cl.translation(), Vector3d::Ones());
  EXPECT_EQ(kf.Twc().translation(), Vector3d::Constant(2));

  kf.Reset();
  EXPECT_EQ(kf.is_fixed(), false);
}

/// ============================================================================
PixelGrid MakeTestPixels(const cv::Size& image_size, int cell_size) {
  const cv::Size grid_size{image_size.width / cell_size,
                           image_size.height / cell_size};
  PixelGrid pixels(grid_size);
  for (int gr = 0; gr < grid_size.height; ++gr) {
    for (int gc = 0; gc < grid_size.width; ++gc) {
      auto& px = pixels.at(gr, gc);
      px.x = gc * cell_size + cell_size / 2;
      px.y = gr * cell_size + cell_size / 2;
    }
  }
  return pixels;
}

struct KeyframeBench {
  void SetUp() {
    image = MakeRandMat8U(kImageSize);
    depth = cv::Mat::ones(image.rows, image.cols, CV_32FC1);
    camera = Camera{{image.cols, image.rows}, Eigen::Array4d::Ones(), 0};

    MakeImagePyramid(image, kNumLevels, images);
    frame = Frame(images, Sophus::SE3d{});
    pixels = MakeTestPixels({image.cols, image.rows}, kCellSize);
    keyframe.SetFrame(frame);
    keyframe.Allocate(kNumLevels, pixels.cvsize());
  }

  cv::Mat image;
  cv::Mat depth;
  Camera camera;
  ImagePyramid images;
  PixelGrid pixels;
  Frame frame;
  Keyframe keyframe;
};

void BM_KeyframeInitPoints(bm::State& state) {
  KeyframeBench b;
  b.SetUp();

  for (auto _ : state) {
    const auto n = b.keyframe.InitPoints(b.pixels, b.camera);
    bm::DoNotOptimize(n);
  }
}
BENCHMARK(BM_KeyframeInitPoints);

void BM_KeyframeInitPatches(bm::State& state) {
  KeyframeBench b;
  b.SetUp();
  b.keyframe.InitPoints(b.pixels, b.camera);

  const int gsize = static_cast<int>(state.range(0));
  for (auto _ : state) {
    const auto n = b.keyframe.InitPatches(gsize);
    bm::DoNotOptimize(n);
  }
}
BENCHMARK(BM_KeyframeInitPatches)->Arg(0)->Arg(1);

void BM_KeyframeInitFromDepths(bm::State& state) {
  KeyframeBench b;
  b.SetUp();
  b.keyframe.Precompute(b.pixels, b.camera);

  for (auto _ : state) {
    const auto n = b.keyframe.InitFromDepth(b.depth);
    bm::DoNotOptimize(n);
  }
}
BENCHMARK(BM_KeyframeInitFromDepths);

}  // namespace
}  // namespace sv::dsol
