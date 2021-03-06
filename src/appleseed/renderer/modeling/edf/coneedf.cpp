
//
// This source file is part of appleseed.
// Visit https://appleseedhq.net/ for additional information and resources.
//
// This software is released under the MIT license.
//
// Copyright (c) 2010-2013 Francois Beaune, Jupiter Jazz Limited
// Copyright (c) 2014-2018 Francois Beaune, The appleseedhq Organization
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

// Interface header.
#include "coneedf.h"

// appleseed.renderer headers.
#include "renderer/modeling/edf/edf.h"
#include "renderer/modeling/input/inputarray.h"

// appleseed.foundation headers.
#include "foundation/math/basis.h"
#include "foundation/math/sampling/mappings.h"
#include "foundation/math/scalar.h"
#include "foundation/math/vector.h"
#include "foundation/utility/api/specializedapiarrays.h"

// Standard headers.
#include <cassert>
#include <cmath>

// Forward declarations.
namespace foundation    { class IAbortSwitch; }
namespace renderer      { class Assembly; }
namespace renderer      { class Project; }

using namespace foundation;

namespace renderer
{

namespace
{
    //
    // Cone-shaped EDF.
    //

    const char* Model = "cone_edf";

    class ConeEDF
      : public EDF
    {
      public:
        ConeEDF(
            const char*             name,
            const ParamArray&       params)
          : EDF(name, params)
        {
            m_inputs.declare("radiance", InputFormatSpectralIlluminance);
            m_inputs.declare("radiance_multiplier", InputFormatFloat, "1.0");
            m_inputs.declare("exposure", InputFormatFloat, "0.0");
            m_inputs.declare("angle", InputFormatFloat, "90.0");
        }

        void release() override
        {
            delete this;
        }

        const char* get_model() const override
        {
            return Model;
        }

        bool on_render_begin(
            const Project&          project,
            const BaseGroup*        parent,
            OnRenderBeginRecorder&  recorder,
            IAbortSwitch*           abort_switch) override
        {
            if (!EDF::on_render_begin(project, parent, recorder, abort_switch))
                return false;

            check_non_zero_emission("radiance", "radiance_multiplier");

            InputValues values;
            get_inputs().evaluate_uniforms(&values);
            m_exposure_multiplier = std::pow(2.0f, values.m_exposure);

            m_cos_half_angle = std::cos(deg_to_rad(m_params.get_required<float>("angle", 90.0f) / 2.0f));

            return true;
        }

        void sample(
            SamplingContext&        sampling_context,
            const void*             data,
            const Vector3f&         geometric_normal,
            const Basis3f&          shading_basis,
            const Vector2f&         s,
            Vector3f&               outgoing,
            Spectrum&               value,
            float&                  probability) const override
        {
            assert(is_normalized(geometric_normal));

            const Vector3f wo = sample_cone_uniform(s, m_cos_half_angle);
            outgoing = shading_basis.transform_to_parent(wo);

            const InputValues* values = static_cast<const InputValues*>(data);
            value = values->m_radiance;
            value *= values->m_radiance_multiplier * m_exposure_multiplier;

            probability = sample_cone_uniform_pdf(m_cos_half_angle);
            assert(probability > 0.0f);
        }

        void evaluate(
            const void*             data,
            const Vector3f&         geometric_normal,
            const Basis3f&          shading_basis,
            const Vector3f&         outgoing,
            Spectrum&               value) const override
        {
            assert(is_normalized(geometric_normal));
            assert(is_normalized(outgoing));

            // No emission outside the cone.
            const float cos_on = dot(outgoing, shading_basis.get_normal());
            if (cos_on <= m_cos_half_angle)
            {
                value.set(0.0f);
                return;
            }

            const InputValues* values = static_cast<const InputValues*>(data);
            value = values->m_radiance;
            value *= values->m_radiance_multiplier * m_exposure_multiplier;
        }

        void evaluate(
            const void*             data,
            const Vector3f&         geometric_normal,
            const Basis3f&          shading_basis,
            const Vector3f&         outgoing,
            Spectrum&               value,
            float&                  probability) const override
        {
            assert(is_normalized(geometric_normal));
            assert(is_normalized(outgoing));

            // No emission outside the cone.
            const float cos_on = dot(outgoing, shading_basis.get_normal());
            if (cos_on <= m_cos_half_angle)
            {
                value.set(0.0f);
                probability = 0.0f;
                return;
            }

            const InputValues* values = static_cast<const InputValues*>(data);
            value = values->m_radiance;
            value *= values->m_radiance_multiplier * m_exposure_multiplier;

            probability = sample_cone_uniform_pdf(m_cos_half_angle);
            assert(probability > 0.0f);
        }

        float evaluate_pdf(
            const void*             data,
            const Vector3f&         geometric_normal,
            const Basis3f&          shading_basis,
            const Vector3f&         outgoing) const override
        {
            assert(is_normalized(geometric_normal));
            assert(is_normalized(outgoing));

            // No emission outside the cone.
            const float cos_on = dot(outgoing, shading_basis.get_normal());
            if (cos_on <= m_cos_half_angle)
                return 0.0f;

            return sample_cone_uniform_pdf(m_cos_half_angle);
        }

        float get_uncached_max_contribution() const override
        {
            return get_max_contribution("radiance", "radiance_multiplier", "exposure");
        }

      private:
        typedef ConeEDFInputValues InputValues;

        float m_exposure_multiplier;
        float m_cos_half_angle;
    };
}


//
// ConeEDFFactory class implementation.
//

void ConeEDFFactory::release()
{
    delete this;
}

const char* ConeEDFFactory::get_model() const
{
    return Model;
}

Dictionary ConeEDFFactory::get_model_metadata() const
{
    return
        Dictionary()
            .insert("name", Model)
            .insert("label", "Cone EDF");
}

DictionaryArray ConeEDFFactory::get_input_metadata() const
{
    DictionaryArray metadata;

    metadata.push_back(
        Dictionary()
            .insert("name", "radiance")
            .insert("label", "Radiance")
            .insert("type", "colormap")
            .insert("entity_types",
                Dictionary()
                    .insert("color", "Colors")
                    .insert("texture_instance", "Texture Instances"))
            .insert("use", "required")
            .insert("default", "1.0"));

    metadata.push_back(
        Dictionary()
            .insert("name", "radiance_multiplier")
            .insert("label", "Radiance Multiplier")
            .insert("type", "colormap")
            .insert("entity_types",
                Dictionary().insert("texture_instance", "Texture Instances"))
            .insert("use", "optional")
            .insert("default", "1.0"));

    metadata.push_back(
        Dictionary()
            .insert("name", "exposure")
            .insert("label", "Exposure")
            .insert("type", "numeric")
            .insert("min",
                Dictionary()
                    .insert("value", "-8.0")
                    .insert("type", "soft"))
            .insert("max",
                Dictionary()
                    .insert("value", "8.0")
                    .insert("type", "soft"))
            .insert("use", "optional")
            .insert("default", "0.0")
            .insert("help", "Exposure"));

    metadata.push_back(
        Dictionary()
            .insert("name", "angle")
            .insert("label", "Angle")
            .insert("type", "numeric")
            .insert("min",
                Dictionary()
                    .insert("value", "0.0")
                    .insert("type", "hard"))
            .insert("max",
                Dictionary()
                    .insert("value", "180.0")
                    .insert("type", "hard"))
            .insert("use", "required")
            .insert("default", "90.0"));

    add_common_input_metadata(metadata);

    return metadata;
}

auto_release_ptr<EDF> ConeEDFFactory::create(
    const char*         name,
    const ParamArray&   params) const
{
    return auto_release_ptr<EDF>(new ConeEDF(name, params));
}

}   // namespace renderer
