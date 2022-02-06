///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <QDebug>

#include "czml.h"
#include "mapsettings.h"
#include "mapmodel.h"

#include "util/units.h"

CZML::CZML(const MapSettings *settings) :
    m_settings(settings)
{
}

QJsonObject CZML::init()
{
    QString start = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    QString stop = QDateTime::currentDateTimeUtc().addSecs(60*60).toString(Qt::ISODate);
    QString interval = QString("%1/%2").arg(start).arg(stop);

    QJsonObject spec {
        {"interval", interval},
        {"currentTime", start},
        {"range", "UNBOUNDED"}
    };
    QJsonObject doc {
        {"id", "document"},
        {"version", "1.0"},
        {"clock", spec}
    };

    return doc;
}

// Convert a position specified in longitude, latitude in degrees and height in metres above WGS84 ellipsoid in to
// Earth Centered Earth Fixed frame cartesian coordinates
// See Cesium.Cartesian3.fromDegrees
QVector3D CZML::cartesian3FromDegrees(double longitude, double latitude, double height) const
{
    return cartesianFromRadians(Units::degreesToRadians(longitude), Units::degreesToRadians(latitude), height);
}

// FIXME: QVector3D is only float!
// See Cesium.Cartesian3.fromRadians
QVector3D CZML::cartesianFromRadians(double longitude, double latitude, double height) const
{
    QVector3D wgs84RadiiSquared(6378137.0 * 6378137.0, 6378137.0 * 6378137.0, 6356752.3142451793 * 6356752.3142451793);

    double cosLatitude = cos(latitude);
    QVector3D n;
    n.setX(cosLatitude * cos(longitude));
    n.setY(cosLatitude * sin(longitude));
    n.setZ(sin(latitude));
    n.normalize();
    QVector3D k;
    k = wgs84RadiiSquared * n;
    double gamma = sqrt(QVector3D::dotProduct(n, k));
    k = k / gamma;
    n = n * height;
    return k + n;
}

// Convert heading, pitch and roll in degrees to a quaternoin
// See: Cesium.Quaternion.fromHeadingPitchRoll
QQuaternion CZML::fromHeadingPitchRoll(double heading, double pitch, double roll) const
{
    QVector3D xAxis(1, 0, 0);
    QVector3D yAxis(0, 1, 0);
    QVector3D zAxis(0, 0, 1);

    QQuaternion rollQ = QQuaternion::fromAxisAndAngle(xAxis, roll);

    QQuaternion pitchQ = QQuaternion::fromAxisAndAngle(yAxis, -pitch);

    QQuaternion headingQ = QQuaternion::fromAxisAndAngle(zAxis, -heading);

    QQuaternion temp = rollQ * pitchQ;

    return headingQ * temp;
}

// Calculate a transformation matrix from a East, North, Up frame at the given position to Earth Centered Earth Fixed frame
// See: Cesium.Transforms.eastNorthUpToFixedFrame
QMatrix4x4 CZML::eastNorthUpToFixedFrame(QVector3D origin) const
{
    // TODO: Handle special case at centre of earth and poles
    QVector3D up = origin.normalized();
    QVector3D east(-origin.y(), origin.x(), 0.0);
    east.normalize();
    QVector3D north = QVector3D::crossProduct(up, east);
    QMatrix4x4 result(
        east.x(), north.x(), up.x(), origin.x(),
        east.y(), north.y(), up.y(), origin.y(),
        east.z(), north.z(), up.z(), origin.z(),
        0.0, 0.0, 0.0, 1.0
    );
    return result;
}

// Convert 3x3 rotation matrix to a quaternoin
// Although there is a method for this in Qt: QQuaternion::fromRotationMatrix, it seems to
// result in different signs, so the following is based on Cesium code
QQuaternion CZML::fromRotation(QMatrix3x3 mat) const
{
    QQuaternion q;

    double trace = mat(0, 0) + mat(1, 1) + mat(2, 2);

    if (trace > 0.0)
    {
        double root = sqrt(trace + 1.0);
        q.setScalar(0.5 * root);
        root = 0.5 / root;

        q.setX((mat(2,1) - mat(1,2)) * root);
        q.setY((mat(0,2) - mat(2,0)) * root);
        q.setZ((mat(1,0) - mat(0,1)) * root);
    }
    else
    {
        double next[] = {1, 2, 0};
        int i = 0;
        if (mat(1,1) > mat(0,0)) {
            i = 1;
        }
        if (mat(2,2) > mat(0,0) && mat(2,2) > mat(1,1)) {
            i = 2;
        }
        int j = next[i];
        int k = next[j];

        double root = sqrt(mat(i,i) - mat(j,j) - mat(k,k) + 1);
        double quat[] = {0.0, 0.0, 0.0};
        quat[i] = 0.5 * root;
        root = 0.5 / root;

        q.setScalar((mat(j,k) - mat(k,j)) * root);
        quat[j] = (mat(i,j) + mat(j,i)) * root;
        quat[k] = (mat(i,k) + mat(k,i)) * root;
        q.setX(-quat[0]);
        q.setY(-quat[1]);
        q.setZ(-quat[2]);
    }
    return q;
}

// Calculate orientation quaternion for a model (such as an aircraft) based on position and (HPR) heading, pitch and roll (in degrees)
// While Cesium supports specifying orientation as HPR, CZML doesn't currently. See https://github.com/CesiumGS/cesium/issues/5184
// CZML requires the orientation to be in the Earth Centered Earth Fixed (geocentric) reference frame (https://en.wikipedia.org/wiki/Local_tangent_plane_coordinates)
// The orientation therefore depends not only on HPR but also on position
//
// glTF uses a right-handed axis convention; that is, the cross product of right and forward yields up. glTF defines +Y as up, +Z as forward, and -X as right.
// Cesium.Quaternion.fromHeadingPitchRoll  Heading is the rotation about the negative z axis. Pitch is the rotation about the negative y axis. Roll is the rotation about the positive x axis.
QQuaternion CZML::orientation(double longitude, double latitude, double altitude, double heading, double pitch, double roll) const
{
    // Forward direction for gltf models in Cesium seems to be Eastward, rather than Northward, so we adjust heading by -90 degrees
    heading = -90 + heading;

    // Convert position to Earth Centered Earth Fixed (ECEF) frame
    QVector3D positionECEF = cartesian3FromDegrees(longitude, latitude, altitude);

    // Calculate matrix to transform from East, North, Up (ENU) frame to ECEF frame
    QMatrix4x4 enuToECEFTransform = eastNorthUpToFixedFrame(positionECEF);

    // Calculate rotation based on HPR in ENU frame
    QQuaternion hprENU = fromHeadingPitchRoll(heading, pitch, roll);

    // Transform rotation from ENU to ECEF
    QMatrix3x3 hprENU3 = hprENU.toRotationMatrix();
    QMatrix4x4 hprENU4(hprENU3);
    QMatrix4x4 transform = enuToECEFTransform * hprENU4;

    // Convert from 4x4 matrix to 3x3 matrix then to a quaternion
    QQuaternion oq = fromRotation(transform.toGenericMatrix<3,3>());

    return oq;
}

QJsonObject CZML::update(MapItem *mapItem, bool isTarget, bool isSelected)
{
    (void) isTarget;

    // Don't currently use CLIP_TO_GROUND in Cesium due to Jitter bug
    // https://github.com/CesiumGS/cesium/issues/4049
    // Instead we implement our own clipping code in map3d.html
    const QStringList heightReferences = {"NONE", "CLAMP_TO_GROUND", "RELATIVE_TO_GROUND", "NONE"};
    QString dt;

    if (mapItem->m_takenTrackDateTimes.size() > 0) {
        dt = mapItem->m_takenTrackDateTimes.last()->toString(Qt::ISODateWithMs);
    } else {
        dt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    }

    QString id = mapItem->m_name;

    // Keep a hash of the time we first saw each item
    bool existingId = m_ids.contains(id);
    if (!existingId) {
        m_ids.insert(id, dt);
    }

    bool removeObj = false;
    bool fixedPosition = mapItem->m_fixedPosition;

    float displayDistanceMax = std::numeric_limits<float>::max();
    QString image = mapItem->m_image;
    if ((image == "antenna.png") || (image == "antennaam.png") || (image == "antennadab.png") || (image == "antennafm.png") || (image == "antennatime.png")) {
        displayDistanceMax = 1000000;
    }
    if (image == "") {
        // Need to remove this from the map
        removeObj = true;
    }

    QJsonArray coords;
    if (!removeObj)
    {
        if (!fixedPosition && (mapItem->m_predictedTrackCoords.size() > 0))
        {
            QListIterator<QGeoCoordinate *> i(mapItem->m_takenTrackCoords);
            QListIterator<QDateTime *> j(mapItem->m_takenTrackDateTimes);
            while (i.hasNext())
            {
                QGeoCoordinate *c = i.next();
                coords.append(j.next()->toString(Qt::ISODateWithMs));
                coords.append(c->longitude());
                coords.append(c->latitude());
                coords.append(c->altitude());
            }
            if (mapItem->m_predictedTrackCoords.size() > 0)
            {
                QListIterator<QGeoCoordinate *> k(mapItem->m_predictedTrackCoords);
                QListIterator<QDateTime *> l(mapItem->m_predictedTrackDateTimes);
                k.toBack();
                l.toBack();
                while (k.hasPrevious())
                {
                    QGeoCoordinate *c = k.previous();
                    coords.append(l.previous()->toString(Qt::ISODateWithMs));
                    coords.append(c->longitude());
                    coords.append(c->latitude());
                    coords.append(c->altitude());
                }
            }
        }
        else
        {
            // Only send latest position, to reduce processing
            if (!fixedPosition && mapItem->m_positionDateTime.isValid()) {
                coords.push_back(mapItem->m_positionDateTime.toString(Qt::ISODateWithMs));
            }
            coords.push_back(mapItem->m_longitude);
            coords.push_back(mapItem->m_latitude);
            coords.push_back(mapItem->m_altitude);
        }
    }
    else
    {
        coords = m_lastPosition.value(id);
    }
    QJsonObject position {
        {"cartographicDegrees", coords},
    };
    if (!fixedPosition)
    {
        // Don't use forward extrapolation for satellites (with predicted tracks), as
        // it seems to jump about. We use it for AIS and ADS-B that don't have predicted tracks
        if (mapItem->m_predictedTrackCoords.size() == 0)
        {
            // Need 2 different positions to enable extrapolation, otherwise entity may not appear
            bool hasMoved = m_hasMoved.contains(id);
            if (!hasMoved && m_lastPosition.contains(id) && (m_lastPosition.value(id) != coords))
            {
                hasMoved = true;
                m_hasMoved.insert(id, true);
            }
            if (hasMoved)
            {
                position.insert("forwardExtrapolationType", "EXTRAPOLATE");
                position.insert("forwardExtrapolationDuration", 60);
                // Use linear interpolation for now - other two can go crazy with aircraft on the ground
                //position.insert("interpolationAlgorithm", "HERMITE");
                //position.insert("interpolationDegree", "2");
                //position.insert("interpolationAlgorithm", "LAGRANGE");
                //position.insert("interpolationDegree", "5");
            }
            else
            {
                position.insert("forwardExtrapolationType", "HOLD");
            }
        }
        else
        {
            // Interpolation goes wrong at end points
            //position.insert("interpolationAlgorithm", "LAGRANGE");
            //position.insert("interpolationDegree", "5");
            //position.insert("interpolationAlgorithm", "HERMITE");
            //position.insert("interpolationDegree", "2");
        }
    }

    QQuaternion q = orientation(mapItem->m_longitude, mapItem->m_latitude, mapItem->m_altitude,
                                mapItem->m_heading, mapItem->m_pitch, mapItem->m_roll);
    QJsonArray quaternion;
    if (!fixedPosition && mapItem->m_orientationDateTime.isValid()) {
        quaternion.push_back(mapItem->m_orientationDateTime.toString(Qt::ISODateWithMs));
    }
    quaternion.push_back(q.x());
    quaternion.push_back(q.y());
    quaternion.push_back(q.z());
    quaternion.push_back(q.scalar());

    QJsonObject orientation {
        {"unitQuaternion", quaternion},
        {"forwardExtrapolationType", "HOLD"}, // If we extrapolate, aircraft tend to spin around
        {"forwardExtrapolationDuration", 60},
       // {"interpolationAlgorithm", "LAGRANGE"}
    };
    QJsonObject orientationPosition {
        {"velocityReference", "#position"},
    };
    QJsonObject noPosition {
        {"cartographicDegrees", coords},
        {"forwardExtrapolationType", "NONE"}
    };

    // Point
    QColor pointColor = QColor::fromRgba(mapItem->m_itemSettings->m_3DPointColor);
    QJsonArray pointRGBA {
        pointColor.red(), pointColor.green(), pointColor.blue(), pointColor.alpha()
    };
    QJsonObject pointColorObj {
        {"rgba", pointRGBA}
    };
    QJsonObject point {
        {"pixelSize", 8},
        {"color", pointColorObj},
        {"heightReference", heightReferences[mapItem->m_altitudeReference]},
        {"show", mapItem->m_itemSettings->m_enabled && mapItem->m_itemSettings->m_display3DPoint}
    };
    // If clamping to ground, we need to disable depth test, so part of the point isn't clipped
    // However, when the point isn't clamped to ground, we shouldn't use this, otherwise
    // the point will become visible through the globe
    if (mapItem->m_altitudeReference == 1) {
        point.insert("disableDepthTestDistance", 100000000);
    }

    // Model
    QJsonArray node0Cartesian {
        {0.0, mapItem->m_modelAltitudeOffset, 0.0}
    };
    QJsonObject node0Translation {
        {"cartesian", node0Cartesian}
    };
    QJsonObject node0Transform {
        {"translation", node0Translation}
    };
    QJsonObject nodeTransforms {
        {"node0", node0Transform},
    };
    QJsonObject model {
        {"gltf", m_settings->m_modelURL + mapItem->m_model},
        {"incrementallyLoadTextures", false}, // Aircraft will flash as they appear without textures if this is the default of true
        {"heightReference", heightReferences[mapItem->m_altitudeReference]},
        {"runAnimations", false},
        {"show", mapItem->m_itemSettings->m_enabled && mapItem->m_itemSettings->m_display3DModel},
        {"minimumPixelSize", mapItem->m_itemSettings->m_3DModelMinPixelSize},
        {"maximumScale", 20000} // Stop it getting too big when zoomed really far out
    };
    if (mapItem->m_modelAltitudeOffset != 0.0) {
        model.insert("nodeTransformations", nodeTransforms);
    }

    // Path
    QColor pathColor = QColor::fromRgba(mapItem->m_itemSettings->m_3DTrackColor);
    QJsonArray pathColorRGBA {
        pathColor.red(), pathColor.green(), pathColor.blue(), pathColor.alpha()
    };
    QJsonObject pathColorObj {
        {"rgba", pathColorRGBA}
    };
    // Paths can't be clamped to ground, so AIS paths can be underground if terrain is used
    // See: https://github.com/CesiumGS/cesium/issues/7133
    QJsonObject pathSolidColorMaterial {
        {"color", pathColorObj}
    };
    QJsonObject pathMaterial {
        {"solidColor", pathSolidColorMaterial}
    };
    bool showPath = mapItem->m_itemSettings->m_enabled
                        && mapItem->m_itemSettings->m_display3DTrack
                        && (    m_settings->m_displayAllGroundTracks
                            || (m_settings->m_displaySelectedGroundTracks && isSelected));
    QJsonObject path {
        // We want full paths for sat tracker, so leadTime and trailTime should be 0
        // Should be configurable.. 6000=100mins ~> 1 orbit for LEO
        //{"leadTime", "6000"},
        //{"trailTime", "6000"},
        {"width", "3"},
        {"material", pathMaterial},
        {"show", showPath}
    };

    // Label
    QJsonArray labelPixelOffsetArray {
        20, 0
    };
    QJsonObject labelPixelOffset {
        {"cartesian2", labelPixelOffsetArray}
    };
    QJsonArray labelEyeOffsetArray {
        0, mapItem->m_labelAltitudeOffset, 0     // Position above the object, dependent on the height of the model
    };
    QJsonObject labelEyeOffset {
        {"cartesian", labelEyeOffsetArray}
    };
    QJsonObject labelHorizontalOrigin  {
        {"horizontalOrigin", "LEFT"}
    };
    QJsonArray labelDisplayDistance {
        0, displayDistanceMax
    };
    QJsonObject labelDistanceDisplayCondition {
        {"distanceDisplayCondition", labelDisplayDistance}
    };
    QJsonObject label {
        {"text", mapItem->m_label},
        {"show", m_settings->m_displayNames && mapItem->m_itemSettings->m_enabled && mapItem->m_itemSettings->m_display3DLabel},
        {"scale", 0.5},
        {"pixelOffset", labelPixelOffset},
        {"eyeOffset", labelEyeOffset},
        {"verticalOrigin",  "BASELINE"},
        {"horizontalOrigin",  "LEFT"},
        {"heightReference", heightReferences[mapItem->m_altitudeReference]},
    };
    if (displayDistanceMax != std::numeric_limits<float>::max())
    {
        label.insert("disableDepthTestDistance", 100000000.0);
        label.insert("distanceDisplayCondition", labelDistanceDisplayCondition);
    }

    // Use billboard for APRS as we don't currently have 3D objects
    QString imageURL = mapItem->m_image;
    if (imageURL.startsWith("qrc://")) {
        imageURL = imageURL.mid(6); // Redirect to our embedded webserver, which will check resources
    }
    QJsonObject billboard {
        {"image", imageURL},
        {"heightReference", heightReferences[mapItem->m_altitudeReference]},
        {"verticalOrigin", "BOTTOM"} // To stop it being cut in half when zoomed out
    };
    if (mapItem->m_altitudeReference == 1) {
        billboard.insert("disableDepthTestDistance", 100000000);
    }

    QJsonObject obj {
        {"id", id}         // id must be unique
    };

    if (!removeObj)
    {
        obj.insert("position", position);
        if (!fixedPosition)
        {
            if (mapItem->m_useHeadingPitchRoll) {
                obj.insert("orientation", orientation);
            } else {
                obj.insert("orientation", orientationPosition);
            }
        }
        obj.insert("point", point);
        if (!mapItem->m_model.isEmpty()) {
            obj.insert("model", model);
        } else {
            obj.insert("billboard", billboard);
        }
        obj.insert("label", label);
        obj.insert("description", mapItem->m_text);
        if (!fixedPosition) {
            obj.insert("path", path);
        }

        if (!fixedPosition)
        {
            if (mapItem->m_takenTrackDateTimes.size() > 0 && mapItem->m_predictedTrackDateTimes.size() > 0)
            {
                QString availability = QString("%1/%2")
                                        .arg(mapItem->m_takenTrackDateTimes.last()->toString(Qt::ISODateWithMs))
                                        .arg(mapItem->m_predictedTrackDateTimes.last()->toString(Qt::ISODateWithMs));
                obj.insert("availability", availability);
            }
            else
            {
                QString oneMin = QDateTime::currentDateTimeUtc().addSecs(60).toString(Qt::ISODateWithMs);
                QString createdToNow = QString("%1/%2").arg(m_ids[id]).arg(oneMin); // From when object was created to now
                obj.insert("availability", createdToNow);
            }
        }
        m_lastPosition.insert(id, coords);
    }
    else
    {
        // Disable forward extrapolation
        obj.insert("position", noPosition);
    }

    // Use our own clipping routine, due to
    // https://github.com/CesiumGS/cesium/issues/4049
    if (mapItem->m_altitudeReference == 3) {
        obj.insert("altitudeReference", "CLIP_TO_GROUND");
    }

    //qDebug() << obj;

    return obj;
}