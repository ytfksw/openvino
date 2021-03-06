// Copyright (C) 2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "layer_transformation.hpp"

#include <string>
#include <sstream>
#include <memory>

#include <gtest/gtest.h>

#include <transformations/utils/utils.hpp>
#include <transformations/init_node_info.hpp>
#include <low_precision/relu.hpp>

#include "common_test_utils/ngraph_test_utils.hpp"
#include "ngraph_functions/low_precision_transformations/common/dequantization_operations.hpp"
#include "ngraph_functions/low_precision_transformations/relu_function.hpp"
#include "simple_low_precision_transformer.hpp"

namespace {

using namespace testing;
using namespace ngraph::pass;

class ReluTransformationTestValues {
public:
    class Actual {
    public:
        ngraph::element::Type precisionBeforeDequantization;
        ngraph::builder::subgraph::DequantizationOperations dequantization;
    };

    class Expected {
    public:
        ngraph::element::Type precisionBeforeDequantization;
        ngraph::builder::subgraph::DequantizationOperations dequantizationBefore;
        ngraph::element::Type precisionAfterOperation;
        ngraph::builder::subgraph::DequantizationOperations dequantizationAfter;
    };

    ngraph::Shape shape;
    ngraph::pass::low_precision::LayerTransformation::Params params;
    Actual actual;
    Expected expected;
};

class ReluTransformation : public LayerTransformation, public testing::WithParamInterface<ReluTransformationTestValues> {
public:
    void SetUp() override {
        const ReluTransformationTestValues testValues = GetParam();

        actualFunction = ngraph::builder::subgraph::ReluFunction::getOriginal(
            testValues.shape,
            testValues.actual.precisionBeforeDequantization,
            testValues.actual.dequantization);

        SimpleLowPrecisionTransformer transformer;
        transformer.add<ngraph::pass::low_precision::ReluTransformation, ngraph::opset1::Relu>(testValues.params);
        transformer.transform(actualFunction);

        referenceFunction = ngraph::builder::subgraph::ReluFunction::getReference(
            testValues.shape,
            testValues.expected.precisionBeforeDequantization,
            testValues.expected.dequantizationBefore,
            testValues.expected.precisionAfterOperation,
            testValues.expected.dequantizationAfter);
    }

    static std::string getTestCaseName(testing::TestParamInfo<ReluTransformationTestValues> obj) {
        const ReluTransformationTestValues testValues = obj.param;

        std::ostringstream result;
        result <<
            testValues.shape << "_" <<
            testValues.actual.precisionBeforeDequantization << "_" <<
            testValues.actual.dequantization << "_" <<
            testValues.expected.dequantizationBefore;
        return result.str();
    }

protected:
    std::shared_ptr<ngraph::Function> actualFunction;
    std::shared_ptr<ngraph::Function> referenceFunction;
};

TEST_P(ReluTransformation, CompareFunctions) {
    actualFunction->validate_nodes_and_infer_types();
    auto res = compare_functions(referenceFunction, actualFunction, true, true, true);
    ASSERT_TRUE(res.first) << res.second;
}

const std::vector<ngraph::Shape> shapes = {
    { 1, 3, 16, 16 }
};

const std::vector<ReluTransformationTestValues> testValues = {
    // U8: no subtract
    {
        ngraph::Shape({ 1, 3, 16, 16 }),
        LayerTransformation::createParamsU8I8(),
        {
            ngraph::element::u8,
            {{ngraph::element::f32}, {}, {0.1f}}
        },
        {
            ngraph::element::u8,
            {{}, {}, {}},
            ngraph::element::u8,
            {{ngraph::element::f32}, {}, {0.1f}}
        }
    },
    // U8: no subtract
    {
        ngraph::Shape({ 1, 3, 16, 16 }),
        LayerTransformation::createParamsU8I8(),
        {
            ngraph::element::u8,
            {{ngraph::element::f32}, {}, {{0.1f, 0.2f, 0.3f}}}
        },
        {
            ngraph::element::u8,
            {{}, {}, {}},
            ngraph::element::u8,
            {{ngraph::element::f32}, {}, {{0.1f, 0.2f, 0.3f}}}
        }
    },
    // U8: no subtract
    {
        ngraph::Shape({ 1, 3, 16, 16 }),
        LayerTransformation::createParamsU8I8(),
        {
            ngraph::element::u8,
            {{ngraph::element::f32}, {}, {{0.1f, -0.2f, 0.3f}}}
        },
        {
            ngraph::element::u8,
            {{ngraph::element::f32}, {}, {{0.1f, -0.2f, 0.3f}}},
            ngraph::element::f32,
            {{}, {}, {}}
        }
    },
    // I8: no subtract
    {
        ngraph::Shape({ 1, 3, 16, 16 }),
        LayerTransformation::createParamsI8I8(),
        {
            ngraph::element::i8,
            {{ngraph::element::f32}, {}, {0.1f}}
        },
        {
            ngraph::element::i8,
            {{}, {}, {}},
            ngraph::element::i8,
            {{ngraph::element::f32}, {}, {0.1f}}
        }
    },
    // U8: with subtract value
    {
        ngraph::Shape({ 1, 3, 16, 16 }),
        LayerTransformation::createParamsU8I8(),
        {
            ngraph::element::u8,
            {{ngraph::element::f32}, { 128 }, {0.1f}}
        },
        {
            ngraph::element::u8,
            {{}, { {128}, ngraph::element::f32, {}, false }, {}},
            ngraph::element::f32,
            {{}, {}, {0.1f}}
        }
    },
    // I8: with subtract value
    {
        ngraph::Shape({ 1, 3, 16, 16 }),
        LayerTransformation::createParamsI8I8().setSupportAsymmetricQuantization(true),
        {
            ngraph::element::i8,
            {{ngraph::element::f32}, { 127 }, {0.1f}}
        },
        {
            ngraph::element::i8,
            {{}, { {127}, ngraph::element::f32, {}, false }, {}},
            ngraph::element::f32,
            {{}, {}, {0.1f}}
        }
    },
    // I8: with subtract value
    {
        ngraph::Shape({ 1, 3, 16, 16 }),
        LayerTransformation::createParamsI8I8().setSupportAsymmetricQuantization(false),
        {
            ngraph::element::i8,
            {{ngraph::element::f32}, { 127 }, {0.1f}}
        },
        {
            ngraph::element::i8,
            {{ngraph::element::f32}, { 127 }, {0.1f}},
            ngraph::element::f32,
            {{}, {}, {}}
        }
    },
    // U8: empty
    {
        ngraph::Shape({ 1, 3, 16, 16 }),
        LayerTransformation::createParamsU8I8(),
        {
            ngraph::element::u8,
            {}
        },
        {
            ngraph::element::u8,
            {},
            ngraph::element::u8,
            {}
        }
    },
    // FP32: empty
    {
        ngraph::Shape({ 1, 3, 16, 16 }),
        LayerTransformation::createParamsU8I8(),
        {
            ngraph::element::f32,
            {}
        },
        {
            ngraph::element::f32,
            {},
            ngraph::element::f32,
            {}
        }
    }
};

INSTANTIATE_TEST_CASE_P(
    LPT,
    ReluTransformation,
    ::testing::ValuesIn(testValues),
    ReluTransformation::getTestCaseName);

} // namespace
