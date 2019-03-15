

// Include GLGizmoBase.hpp before I18N.hpp as it includes some libigl code, which overrides our localization "L" macro.
#include "GLGizmoScale.hpp"

#include <GL/glew.h>

namespace Slic3r {
namespace GUI {


const float GLGizmoScale3D::Offset = 5.0f;

#if ENABLE_SVG_ICONS
GLGizmoScale3D::GLGizmoScale3D(GLCanvas3D& parent, const std::string& icon_filename, unsigned int sprite_id)
    : GLGizmoBase(parent, icon_filename, sprite_id)
#else
GLGizmoScale3D::GLGizmoScale3D(GLCanvas3D& parent, unsigned int sprite_id)
    : GLGizmoBase(parent, sprite_id)
#endif // ENABLE_SVG_ICONS
    , m_scale(Vec3d::Ones())
    , m_snap_step(0.05)
    , m_starting_scale(Vec3d::Ones())
{
}

bool GLGizmoScale3D::on_init()
{
    for (int i = 0; i < 10; ++i)
    {
        m_grabbers.push_back(Grabber());
    }

    double half_pi = 0.5 * (double)PI;

    // x axis
    m_grabbers[0].angles(1) = half_pi;
    m_grabbers[1].angles(1) = half_pi;

    // y axis
    m_grabbers[2].angles(0) = half_pi;
    m_grabbers[3].angles(0) = half_pi;

    m_shortcut_key = WXK_CONTROL_S;

    return true;
}

std::string GLGizmoScale3D::on_get_name() const
{
    return L("Scale [S]");
}

void GLGizmoScale3D::on_start_dragging(const GLCanvas3D::Selection& selection)
{
    if (m_hover_id != -1)
    {
        m_starting_drag_position = m_grabbers[m_hover_id].center;
        m_starting_box = selection.get_bounding_box();
    }
}

void GLGizmoScale3D::on_update(const UpdateData& data, const GLCanvas3D::Selection& selection)
{
    if ((m_hover_id == 0) || (m_hover_id == 1))
        do_scale_x(data);
    else if ((m_hover_id == 2) || (m_hover_id == 3))
        do_scale_y(data);
    else if ((m_hover_id == 4) || (m_hover_id == 5))
        do_scale_z(data);
    else if (m_hover_id >= 6)
        do_scale_uniform(data);
}

void GLGizmoScale3D::on_render(const GLCanvas3D::Selection& selection) const
{
    bool single_instance = selection.is_single_full_instance();
    bool single_volume = selection.is_single_modifier() || selection.is_single_volume();
    bool single_selection = single_instance || single_volume;

    Vec3f scale = 100.0f * Vec3f::Ones();
    if (single_instance)
        scale = 100.0f * selection.get_volume(*selection.get_volume_idxs().begin())->get_instance_scaling_factor().cast<float>();
    else if (single_volume)
        scale = 100.0f * selection.get_volume(*selection.get_volume_idxs().begin())->get_volume_scaling_factor().cast<float>();

    if ((single_selection && ((m_hover_id == 0) || (m_hover_id == 1))) || m_grabbers[0].dragging || m_grabbers[1].dragging)
        set_tooltip("X: " + format(scale(0), 4) + "%");
    else if (!m_grabbers[0].dragging && !m_grabbers[1].dragging && ((m_hover_id == 0) || (m_hover_id == 1)))
        set_tooltip("X");
    else if ((single_selection && ((m_hover_id == 2) || (m_hover_id == 3))) || m_grabbers[2].dragging || m_grabbers[3].dragging)
        set_tooltip("Y: " + format(scale(1), 4) + "%");
    else if (!m_grabbers[2].dragging && !m_grabbers[3].dragging && ((m_hover_id == 2) || (m_hover_id == 3)))
        set_tooltip("Y");
    else if ((single_selection && ((m_hover_id == 4) || (m_hover_id == 5))) || m_grabbers[4].dragging || m_grabbers[5].dragging)
        set_tooltip("Z: " + format(scale(2), 4) + "%");
    else if (!m_grabbers[4].dragging && !m_grabbers[5].dragging && ((m_hover_id == 4) || (m_hover_id == 5)))
        set_tooltip("Z");
    else if ((single_selection && ((m_hover_id == 6) || (m_hover_id == 7) || (m_hover_id == 8) || (m_hover_id == 9)))
        || m_grabbers[6].dragging || m_grabbers[7].dragging || m_grabbers[8].dragging || m_grabbers[9].dragging)
    {
        std::string tooltip = "X: " + format(scale(0), 4) + "%\n";
        tooltip += "Y: " + format(scale(1), 4) + "%\n";
        tooltip += "Z: " + format(scale(2), 4) + "%";
        set_tooltip(tooltip);
    }
    else if (!m_grabbers[6].dragging && !m_grabbers[7].dragging && !m_grabbers[8].dragging && !m_grabbers[9].dragging &&
        ((m_hover_id == 6) || (m_hover_id == 7) || (m_hover_id == 8) || (m_hover_id == 9)))
        set_tooltip("X/Y/Z");

    ::glClear(GL_DEPTH_BUFFER_BIT);
    ::glEnable(GL_DEPTH_TEST);

    BoundingBoxf3 box;
    Transform3d transform = Transform3d::Identity();
    Vec3d angles = Vec3d::Zero();
    Transform3d offsets_transform = Transform3d::Identity();

    Vec3d grabber_size = Vec3d::Zero();

    if (single_instance)
    {
        // calculate bounding box in instance local reference system
        const GLCanvas3D::Selection::IndicesList& idxs = selection.get_volume_idxs();
        for (unsigned int idx : idxs)
        {
            const GLVolume* vol = selection.get_volume(idx);
            box.merge(vol->bounding_box.transformed(vol->get_volume_transformation().get_matrix()));
        }

        // gets transform from first selected volume
        const GLVolume* v = selection.get_volume(*idxs.begin());
        transform = v->get_instance_transformation().get_matrix();
        // gets angles from first selected volume
        angles = v->get_instance_rotation();
        // consider rotation+mirror only components of the transform for offsets
        offsets_transform = Geometry::assemble_transform(Vec3d::Zero(), angles, Vec3d::Ones(), v->get_instance_mirror());
        grabber_size = v->get_instance_transformation().get_matrix(true, true, false, true) * box.size();
    }
    else if (single_volume)
    {
        const GLVolume* v = selection.get_volume(*selection.get_volume_idxs().begin());
        box = v->bounding_box;
        transform = v->world_matrix();
        angles = Geometry::extract_euler_angles(transform);
        // consider rotation+mirror only components of the transform for offsets
        offsets_transform = Geometry::assemble_transform(Vec3d::Zero(), angles, Vec3d::Ones(), v->get_instance_mirror());
        grabber_size = v->get_volume_transformation().get_matrix(true, true, false, true) * box.size();
    }
    else
    {
        box = selection.get_bounding_box();
        grabber_size = box.size();
    }

    m_box = box;

    const Vec3d& center = m_box.center();
    Vec3d offset_x = offsets_transform * Vec3d((double)Offset, 0.0, 0.0);
    Vec3d offset_y = offsets_transform * Vec3d(0.0, (double)Offset, 0.0);
    Vec3d offset_z = offsets_transform * Vec3d(0.0, 0.0, (double)Offset);

    // x axis
    m_grabbers[0].center = transform * Vec3d(m_box.min(0), center(1), center(2)) - offset_x;
    m_grabbers[1].center = transform * Vec3d(m_box.max(0), center(1), center(2)) + offset_x;
    ::memcpy((void*)m_grabbers[0].color, (const void*)&AXES_COLOR[0], 3 * sizeof(float));
    ::memcpy((void*)m_grabbers[1].color, (const void*)&AXES_COLOR[0], 3 * sizeof(float));

    // y axis
    m_grabbers[2].center = transform * Vec3d(center(0), m_box.min(1), center(2)) - offset_y;
    m_grabbers[3].center = transform * Vec3d(center(0), m_box.max(1), center(2)) + offset_y;
    ::memcpy((void*)m_grabbers[2].color, (const void*)&AXES_COLOR[1], 3 * sizeof(float));
    ::memcpy((void*)m_grabbers[3].color, (const void*)&AXES_COLOR[1], 3 * sizeof(float));

    // z axis
    m_grabbers[4].center = transform * Vec3d(center(0), center(1), m_box.min(2)) - offset_z;
    m_grabbers[5].center = transform * Vec3d(center(0), center(1), m_box.max(2)) + offset_z;
    ::memcpy((void*)m_grabbers[4].color, (const void*)&AXES_COLOR[2], 3 * sizeof(float));
    ::memcpy((void*)m_grabbers[5].color, (const void*)&AXES_COLOR[2], 3 * sizeof(float));

    // uniform
    m_grabbers[6].center = transform * Vec3d(m_box.min(0), m_box.min(1), center(2)) - offset_x - offset_y;
    m_grabbers[7].center = transform * Vec3d(m_box.max(0), m_box.min(1), center(2)) + offset_x - offset_y;
    m_grabbers[8].center = transform * Vec3d(m_box.max(0), m_box.max(1), center(2)) + offset_x + offset_y;
    m_grabbers[9].center = transform * Vec3d(m_box.min(0), m_box.max(1), center(2)) - offset_x + offset_y;
    for (int i = 6; i < 10; ++i)
    {
        ::memcpy((void*)m_grabbers[i].color, (const void*)m_highlight_color, 3 * sizeof(float));
    }

    // sets grabbers orientation
    for (int i = 0; i < 10; ++i)
    {
        m_grabbers[i].angles = angles;
    }

    ::glLineWidth((m_hover_id != -1) ? 2.0f : 1.5f);

    float grabber_mean_size = (float)(grabber_size(0) + grabber_size(1) + grabber_size(2)) / 3.0f;

    if (m_hover_id == -1)
    {
        // draw connections
        if (m_grabbers[0].enabled && m_grabbers[1].enabled)
        {
            ::glColor3fv(m_grabbers[0].color);
            render_grabbers_connection(0, 1);
        }
        if (m_grabbers[2].enabled && m_grabbers[3].enabled)
        {
            ::glColor3fv(m_grabbers[2].color);
            render_grabbers_connection(2, 3);
        }
        if (m_grabbers[4].enabled && m_grabbers[5].enabled)
        {
            ::glColor3fv(m_grabbers[4].color);
            render_grabbers_connection(4, 5);
        }
        ::glColor3fv(m_base_color);
        render_grabbers_connection(6, 7);
        render_grabbers_connection(7, 8);
        render_grabbers_connection(8, 9);
        render_grabbers_connection(9, 6);
        // draw grabbers
        render_grabbers(grabber_mean_size);
    }
    else if ((m_hover_id == 0) || (m_hover_id == 1))
    {
        // draw connection
        ::glColor3fv(m_grabbers[0].color);
        render_grabbers_connection(0, 1);
        // draw grabbers
        m_grabbers[0].render(true, grabber_mean_size);
        m_grabbers[1].render(true, grabber_mean_size);
    }
    else if ((m_hover_id == 2) || (m_hover_id == 3))
    {
        // draw connection
        ::glColor3fv(m_grabbers[2].color);
        render_grabbers_connection(2, 3);
        // draw grabbers
        m_grabbers[2].render(true, grabber_mean_size);
        m_grabbers[3].render(true, grabber_mean_size);
    }
    else if ((m_hover_id == 4) || (m_hover_id == 5))
    {
        // draw connection
        ::glColor3fv(m_grabbers[4].color);
        render_grabbers_connection(4, 5);
        // draw grabbers
        m_grabbers[4].render(true, grabber_mean_size);
        m_grabbers[5].render(true, grabber_mean_size);
    }
    else if (m_hover_id >= 6)
    {
        // draw connection
        ::glColor3fv(m_drag_color);
        render_grabbers_connection(6, 7);
        render_grabbers_connection(7, 8);
        render_grabbers_connection(8, 9);
        render_grabbers_connection(9, 6);
        // draw grabbers
        for (int i = 6; i < 10; ++i)
        {
            m_grabbers[i].render(true, grabber_mean_size);
        }
    }
}

void GLGizmoScale3D::on_render_for_picking(const GLCanvas3D::Selection& selection) const
{
    ::glDisable(GL_DEPTH_TEST);

    render_grabbers_for_picking(selection.get_bounding_box());
}

void GLGizmoScale3D::on_render_input_window(float x, float y, float bottom_limit, const GLCanvas3D::Selection& selection)
{
#if !DISABLE_MOVE_ROTATE_SCALE_GIZMOS_IMGUI
    bool single_instance = selection.is_single_full_instance();
    wxString label = _(L("Scale (%)"));

    m_imgui->set_next_window_pos(x, y, ImGuiCond_Always);
    m_imgui->set_next_window_bg_alpha(0.5f);
    m_imgui->begin(label, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    m_imgui->input_vec3("", m_scale * 100.f, 100.0f, "%.2f");
    m_imgui->end();
#endif // !DISABLE_MOVE_ROTATE_SCALE_GIZMOS_IMGUI
}

void GLGizmoScale3D::render_grabbers_connection(unsigned int id_1, unsigned int id_2) const
{
    unsigned int grabbers_count = (unsigned int)m_grabbers.size();
    if ((id_1 < grabbers_count) && (id_2 < grabbers_count))
    {
        ::glBegin(GL_LINES);
        ::glVertex3dv(m_grabbers[id_1].center.data());
        ::glVertex3dv(m_grabbers[id_2].center.data());
        ::glEnd();
    }
}

void GLGizmoScale3D::do_scale_x(const UpdateData& data)
{
    double ratio = calc_ratio(data);
    if (ratio > 0.0)
        m_scale(0) = m_starting_scale(0) * ratio;
}

void GLGizmoScale3D::do_scale_y(const UpdateData& data)
{
    double ratio = calc_ratio(data);
    if (ratio > 0.0)
        m_scale(1) = m_starting_scale(1) * ratio;
}

void GLGizmoScale3D::do_scale_z(const UpdateData& data)
{
    double ratio = calc_ratio(data);
    if (ratio > 0.0)
        m_scale(2) = m_starting_scale(2) * ratio;
}

void GLGizmoScale3D::do_scale_uniform(const UpdateData& data)
{
    double ratio = calc_ratio(data);
    if (ratio > 0.0)
        m_scale = m_starting_scale * ratio;
}

double GLGizmoScale3D::calc_ratio(const UpdateData& data) const
{
    double ratio = 0.0;

    // vector from the center to the starting position
    Vec3d starting_vec = m_starting_drag_position - m_starting_box.center();
    double len_starting_vec = starting_vec.norm();
    if (len_starting_vec != 0.0)
    {
        Vec3d mouse_dir = data.mouse_ray.unit_vector();
        // finds the intersection of the mouse ray with the plane parallel to the camera viewport and passing throught the starting position
        // use ray-plane intersection see i.e. https://en.wikipedia.org/wiki/Line%E2%80%93plane_intersection algebric form
        // in our case plane normal and ray direction are the same (orthogonal view)
        // when moving to perspective camera the negative z unit axis of the camera needs to be transformed in world space and used as plane normal
        Vec3d inters = data.mouse_ray.a + (m_starting_drag_position - data.mouse_ray.a).dot(mouse_dir) / mouse_dir.squaredNorm() * mouse_dir;
        // vector from the starting position to the found intersection
        Vec3d inters_vec = inters - m_starting_drag_position;

        // finds projection of the vector along the staring direction
        double proj = inters_vec.dot(starting_vec.normalized());

        ratio = (len_starting_vec + proj) / len_starting_vec;
    }

    if (data.shift_down)
        ratio = m_snap_step * (double)std::round(ratio / m_snap_step);

    return ratio;
}

} // namespace GUI
} // namespace Slic3r
