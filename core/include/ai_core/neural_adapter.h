/*
 Copyright (C) 2026 Jocer S. <patcex@proton.me>

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with this program.  If not, see <https://www.gnu.org/licenses/>.

 SPDX-License-Identifier: AGPL-3.0 OR Commercial
*/
#pragma once

#include <vector>
#include <cstdint>
#include <optional>
#include <string>

#ifdef USE_NEURAL_NETWORK
// Forward declare ONNX types if available
namespace Ort {
class Session;
class Env;
}
#endif

/**
 * Neural Network Adapter Interface
 * Provides abstraction for different NN backends (ONNX, TFLite, etc.)
 */
struct Tensor {
    std::vector<float> data;
    std::vector<int64_t> shape;
};

enum class ErrorCode : uint8_t {
    Ok = 0,
    ModelNotLoaded,
    InputShapeMismatch,
    InferenceFailed,
    BackendError
};

class INeuralAdapter {
public:
    virtual ~INeuralAdapter() = default;
    
    // Synchronous inference
    virtual std::optional<Tensor> Forward(const Tensor& input) = 0;
    
    // Load model from file
    virtual bool LoadModel(const std::string& path) = 0;
};

// Stub implementation that always fails (for fallback testing)
class StubNeuralAdapter final : public INeuralAdapter {
public:
    std::optional<Tensor> Forward(const Tensor&) override {
        return std::nullopt;
    }
    
    bool LoadModel(const std::string&) override {
        return false;  // Always fail to load
    }
};

#ifdef USE_NEURAL_NETWORK
// ONNX Runtime adapter
class ONNXNeuralAdapter final : public INeuralAdapter {
public:
    ONNXNeuralAdapter();
    ~ONNXNeuralAdapter() override;
    
    bool LoadModel(const std::string& path) override;
    std::optional<Tensor> Forward(const Tensor& input) override;
    
private:
    std::unique_ptr<Ort::Session> session_;
    Ort::Env* env_ = nullptr;
};
#endif