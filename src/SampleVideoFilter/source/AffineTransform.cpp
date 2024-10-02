/*
 * Copyright (C) 2005, 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2014 Google Inc.  All rights reserved.
 *               2010 Dirk Schulze <krit@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "AffineTransform.h"
#include <cmath>
# define M_PI           3.14159265358979323846

void AffineTransform::makeIdentity()
{
    setMatrix(1, 0, 0, 1, 0, 0);
}

void AffineTransform::setMatrix(double a, double b, double c, double d, double e, double f)
{
    m_transform[0] = a;
    m_transform[1] = b;
    m_transform[2] = c;
    m_transform[3] = d;
    m_transform[4] = e;
    m_transform[5] = f;
}

bool AffineTransform::isIdentity() const
{
    return (m_transform[0] == 1 && m_transform[1] == 0
         && m_transform[2] == 0 && m_transform[3] == 1
         && m_transform[4] == 0 && m_transform[5] == 0);
}

double AffineTransform::xScale() const
{
    return std::hypot(m_transform[0], m_transform[1]);
}

double AffineTransform::yScale() const
{
    return std::hypot(m_transform[2], m_transform[3]);
}

static double det(const std::array<double, 6>& transform)
{
    return transform[0] * transform[3] - transform[1] * transform[2];
}

bool AffineTransform::isInvertible() const
{
    double determinant = det(m_transform);

    return std::isfinite(determinant) && determinant != 0;
}

AffineTransform AffineTransform::inverse() const
{
    AffineTransform result;
    double determinant = det(m_transform);
    if (!std::isfinite(determinant) || determinant == 0)
        return result;

    if (isIdentityOrTranslation()) {
        result.m_transform[4] = -m_transform[4];
        result.m_transform[5] = -m_transform[5];
        return result;
    }

    result.m_transform[0] = m_transform[3] / determinant;
    result.m_transform[1] = -m_transform[1] / determinant;
    result.m_transform[2] = -m_transform[2] / determinant;
    result.m_transform[3] = m_transform[0] / determinant;
    result.m_transform[4] = (m_transform[2] * m_transform[5]
                           - m_transform[3] * m_transform[4]) / determinant;
    result.m_transform[5] = (m_transform[1] * m_transform[4]
                           - m_transform[0] * m_transform[5]) / determinant;

    return result;
}


// Multiplies this AffineTransform by the provided AffineTransform - i.e.
// this = this * other;
AffineTransform& AffineTransform::multiply(const AffineTransform& other)
{
    AffineTransform trans;
    
    trans.m_transform[0] = other.m_transform[0] * m_transform[0] + other.m_transform[1] * m_transform[2];
    trans.m_transform[1] = other.m_transform[0] * m_transform[1] + other.m_transform[1] * m_transform[3];
    trans.m_transform[2] = other.m_transform[2] * m_transform[0] + other.m_transform[3] * m_transform[2];
    trans.m_transform[3] = other.m_transform[2] * m_transform[1] + other.m_transform[3] * m_transform[3];
    trans.m_transform[4] = other.m_transform[4] * m_transform[0] + other.m_transform[5] * m_transform[2] + m_transform[4];
    trans.m_transform[5] = other.m_transform[4] * m_transform[1] + other.m_transform[5] * m_transform[3] + m_transform[5];

    *this = trans;
    return *this;
}

AffineTransform& AffineTransform::rotateRadians(double a)
{
    double cosAngle = cos(a);
    double sinAngle = sin(a);
    AffineTransform rot(cosAngle, sinAngle, -sinAngle, cosAngle, 0, 0);

    multiply(rot);
    return *this;
}

AffineTransform& AffineTransform::scale(double s)
{
    return scale(s, s);
}

AffineTransform& AffineTransform::scale(double sx, double sy)
{
    m_transform[0] *= sx;
    m_transform[1] *= sx;
    m_transform[2] *= sy;
    m_transform[3] *= sy;
    return *this;
}

AffineTransform& AffineTransform::scaleNonUniform(double sx, double sy)
{
    return scale(sx, sy);
}

// *this = *this * translation
AffineTransform& AffineTransform::translate(double tx, double ty)
{
    if (isIdentityOrTranslation()) {
        m_transform[4] += tx;
        m_transform[5] += ty;
        return *this;
    }
        
    m_transform[4] += tx * m_transform[0] + ty * m_transform[2];
    m_transform[5] += tx * m_transform[1] + ty * m_transform[3];
    return *this;
}

AffineTransform& AffineTransform::rotateFromVector(double x, double y)
{
    return rotateRadians(atan2(y, x));
}

AffineTransform& AffineTransform::flipX()
{
    return scale(-1, 1);
}

AffineTransform& AffineTransform::flipY()
{
    return scale(1, -1);
}

AffineTransform& AffineTransform::shear(double sx, double sy)
{
    double a = m_transform[0];
    double b = m_transform[1];

    m_transform[0] += sy * m_transform[2];
    m_transform[1] += sy * m_transform[3];
    m_transform[2] += sx * a;
    m_transform[3] += sx * b;

    return *this;
}

void AffineTransform::map(double x, double y, double& x2, double& y2) const
{
    x2 = (m_transform[0] * x + m_transform[2] * y + m_transform[4]);
    y2 = (m_transform[1] * x + m_transform[3] * y + m_transform[5]);
}

void AffineTransform::blend(const AffineTransform& from, double progress)
{
    DecomposedType srA, srB;

    from.decompose(srA);
    this->decompose(srB);

    // If x-axis of one is flipped, and y-axis of the other, convert to an unflipped rotation.
    if ((srA.scaleX < 0 && srB.scaleY < 0) || (srA.scaleY < 0 &&  srB.scaleX < 0)) {
        srA.scaleX = -srA.scaleX;
        srA.scaleY = -srA.scaleY;
        srA.angle += srA.angle < 0 ? M_PI : -M_PI;
    }

    // Don't rotate the long way around.
    srA.angle = fmod(srA.angle, 2 * M_PI);
    srB.angle = fmod(srB.angle, 2 * M_PI);

    if (std::abs(srA.angle - srB.angle) > M_PI) {
        if (srA.angle > srB.angle)
            srA.angle -= M_PI * 2;
        else
            srB.angle -= M_PI * 2;
    }
    
    srA.scaleX += progress * (srB.scaleX - srA.scaleX);
    srA.scaleY += progress * (srB.scaleY - srA.scaleY);
    srA.angle += progress * (srB.angle - srA.angle);
    srA.remainderA += progress * (srB.remainderA - srA.remainderA);
    srA.remainderB += progress * (srB.remainderB - srA.remainderB);
    srA.remainderC += progress * (srB.remainderC - srA.remainderC);
    srA.remainderD += progress * (srB.remainderD - srA.remainderD);
    srA.translateX += progress * (srB.translateX - srA.translateX);
    srA.translateY += progress * (srB.translateY - srA.translateY);

    this->recompose(srA);
}

bool AffineTransform::decompose(DecomposedType& decomp) const
{
    AffineTransform m(*this);
    
    // Compute scaling factors
    double sx = xScale();
    double sy = yScale();
    
    // Compute cross product of transformed unit vectors. If negative,
    // one axis was flipped.
    if (m.a() * m.d() - m.c() * m.b() < 0) {
        // Flip axis with minimum unit vector dot product
        if (m.a() < m.d())
            sx = -sx;
        else
            sy = -sy;
    }
    
    // Remove scale from matrix
    m.scale(1 / sx, 1 / sy);
    
    // Compute rotation
    double angle = atan2(m.b(), m.a());
    
    // Remove rotation from matrix
    m.rotateRadians(-angle);
    
    // Return results    
    decomp.scaleX = sx;
    decomp.scaleY = sy;
    decomp.angle = angle;
    decomp.remainderA = m.a();
    decomp.remainderB = m.b();
    decomp.remainderC = m.c();
    decomp.remainderD = m.d();
    decomp.translateX = m.e();
    decomp.translateY = m.f();
    
    return true;
}

void AffineTransform::recompose(const DecomposedType& decomp)
{
    this->setA(decomp.remainderA);
    this->setB(decomp.remainderB);
    this->setC(decomp.remainderC);
    this->setD(decomp.remainderD);
    this->setE(decomp.translateX);
    this->setF(decomp.translateY);
    this->rotateRadians(decomp.angle);
    this->scale(decomp.scaleX, decomp.scaleY);
}
