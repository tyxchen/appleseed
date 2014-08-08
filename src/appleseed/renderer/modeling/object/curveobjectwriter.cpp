
//
// This source file is part of appleseed.
// Visit http://appleseedhq.net/ for additional information and resources.
//
// This software is released under the MIT license.
//
// Copyright (c) 2014 Srinath Ravichandran, The appleseedhq Organization
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
#include "curveobjectwriter.h"

// appleseed.renderer headers.
#include "renderer/global/globallogger.h"
#include "renderer/global/globaltypes.h"
#include "renderer/kernel/intersection/intersectionsettings.h"
#include "renderer/modeling/object/curveobject.h"

// appleseed.foundation headers.
#include "foundation/core/exceptions/exception.h"
#include "foundation/core/exceptions/exceptionioerror.h"
#include "foundation/platform/defaulttimers.h"
#include "foundation/utility/stopwatch.h"
#include "foundation/utility/string.h"

// Standard headers.
#include <cassert>
#include <cstddef>
#include <fstream>
#include <ostream>
#include <string>

using namespace foundation;
using namespace std;

namespace renderer
{

//
// CurveObjectWriter class implementation.
//

bool CurveObjectWriter::write(
    const CurveObject&  object,
    const char*         filepath)
{
    assert(filepath);

    Stopwatch<DefaultWallclockTimer> stopwatch;
    stopwatch.start();

    ofstream output;
    output.open(filepath);

    if (!output.is_open())
    {
        RENDERER_LOG_ERROR("failed to create curve file %s.", filepath);
        return false;
    }

    const size_t curve_count = object.get_curve_count();

    output << curve_count << endl;
    output << 4 << endl;    // numberof control points

    for (size_t i = 0; i < curve_count; ++i)
    {
        const CurveType& curve = object.get_curve(i);
        const size_t control_point_count = curve.get_control_point_count();

        for (size_t p = 0; p < control_point_count; ++p)
        {
            if (p > 0)
                output << ' ';

            const GVector3& point = curve.get_control_point(p);
            const GScalar width = curve.get_width(p);

            output << point.x << ' ' << point.y << ' ' << point.z << ' ' << width;
        }

        output << endl;
    }

    output.close();

    if (output.bad())
    {
        RENDERER_LOG_ERROR("failed to write curve file %s: i/o error.", filepath);
        return false;
    }

    stopwatch.measure();

    RENDERER_LOG_INFO(
        "wrote curve file %s in %s.",
        filepath,
        pretty_time(stopwatch.get_seconds()).c_str());

    return true;
}

}   // namespace renderer
