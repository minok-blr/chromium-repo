// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/content_visibility_cluster_finalizer.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/history_clusters/core/clustering_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_clusters {
namespace {

class ContentVisibilityClusterFinalizerTest : public ::testing::Test {
 public:
  void SetUp() override {
    cluster_finalizer_ = std::make_unique<ContentVisibilityClusterFinalizer>();
  }

  void TearDown() override { cluster_finalizer_.reset(); }

  void FinalizeCluster(history::Cluster& cluster) {
    cluster_finalizer_->FinalizeCluster(cluster);
  }

 private:
  std::unique_ptr<ContentVisibilityClusterFinalizer> cluster_finalizer_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ContentVisibilityClusterFinalizerTest, OneVisitNoVisibilityScore) {
  // Visit2 has the same URL as Visit1.
  history::ClusterVisit visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://9oo91e.qjz9zk/")));

  history::Cluster cluster;
  cluster.visits = {visit};
  FinalizeCluster(cluster);
  EXPECT_TRUE(cluster.should_show_on_prominent_ui_surfaces);
}

TEST_F(ContentVisibilityClusterFinalizerTest,
       OneVisitNoVisibilityScoreButExplicitlyNotShownBefore) {
  // Visit2 has the same URL as Visit1.
  history::ClusterVisit visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://9oo91e.qjz9zk/")));

  history::Cluster cluster;
  cluster.visits = {visit};
  cluster.should_show_on_prominent_ui_surfaces = false;
  FinalizeCluster(cluster);
  EXPECT_FALSE(cluster.should_show_on_prominent_ui_surfaces);
}

TEST_F(ContentVisibilityClusterFinalizerTest, MultipleVisitsOneBelowThreshold) {
  base::HistogramTester histogram_tester;
  history::ClusterVisit visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://9oo91e.qjz9zk/")));
  visit.annotated_visit.content_annotations.model_annotations.visibility_score =
      1.0;

  history::ClusterVisit visit2 = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://foo.com/")));
  visit2.annotated_visit.content_annotations.model_annotations
      .visibility_score = 0.1;

  history::Cluster cluster;
  cluster.visits = {visit, visit2};
  FinalizeCluster(cluster);
  EXPECT_FALSE(cluster.should_show_on_prominent_ui_surfaces);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.WasClusterFiltered.VisibilityScore", true, 1);
}

TEST_F(ContentVisibilityClusterFinalizerTest, AllVisitsAboveThreshold) {
  base::HistogramTester histogram_tester;
  history::ClusterVisit visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://9oo91e.qjz9zk/")));
  visit.annotated_visit.content_annotations.model_annotations.visibility_score =
      1.0;

  history::ClusterVisit visit2 = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://foo.com/")));
  visit2.annotated_visit.content_annotations.model_annotations
      .visibility_score = 1.0;

  history::Cluster cluster;
  cluster.visits = {visit, visit2};
  FinalizeCluster(cluster);
  EXPECT_TRUE(cluster.should_show_on_prominent_ui_surfaces);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.WasClusterFiltered.VisibilityScore", false, 1);
}

}  // namespace
}  // namespace history_clusters
