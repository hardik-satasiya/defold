package com.dynamo.cr.parted.curve;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.vecmath.Point2d;
import javax.vecmath.Vector2d;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.ITreeSelection;
import org.eclipse.jface.viewers.TreePath;
import org.eclipse.jface.viewers.TreeSelection;

import com.dynamo.cr.editor.core.operations.IMergeableOperation;
import com.dynamo.cr.editor.core.operations.IMergeableOperation.Type;
import com.dynamo.cr.parted.curve.ICurveView.IPresenter;
import com.dynamo.cr.parted.operations.InsertPointOperation;
import com.dynamo.cr.parted.operations.MovePointsOperation;
import com.dynamo.cr.parted.operations.RemovePointsOperation;
import com.dynamo.cr.parted.operations.SetTangentOperation;
import com.dynamo.cr.properties.IPropertyDesc;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.properties.PropertyUtil;
import com.dynamo.cr.properties.types.ValueSpread;
import com.dynamo.cr.sceneed.core.Node;
import com.google.inject.Inject;

public class CurvePresenter implements IPresenter {

    @Inject
    private ICurveView view;
    @Inject
    private IOperationHistory history;
    @Inject
    private IUndoContext undoContext;

    IPropertyModel<Node, IPropertyObjectWorld> propertyModel;
    @SuppressWarnings("unchecked")
    private IPropertyDesc<Node, ? extends IPropertyObjectWorld>[] input = new IPropertyDesc[0];
    @SuppressWarnings("unchecked")
    private IPropertyDesc<Node, ? extends IPropertyObjectWorld>[] oldInput = new IPropertyDesc[0];

    private enum DragMode {
        SELECT,
        MOVE_POINTS,
        SET_TANGENT,
    };
    private DragMode dragMode = DragMode.SELECT;
    private List<Point2d> originalPositions = new ArrayList<Point2d>();
    private ISelection originalSelection = new TreeSelection();
    private ISelection selection = new TreeSelection();
    private Point2d dragStart = new Point2d();
    private Vector2d minDragExtents = new Vector2d();
    private Vector2d hitBoxExtents = new Vector2d();
    private boolean dragging = false;

    public void setModel(IPropertyModel<Node, IPropertyObjectWorld> model) {
        this.propertyModel = model;
        updateInput();
    }

    public ISelection getSelection() {
        return this.selection;
    }

    public void setSelection(ISelection selection) {
        if (!selection.equals(this.selection)) {
            this.selection = selection;
            this.view.setSelection(this.selection);
        }
    }

    @SuppressWarnings("unchecked")
    public void updateInput() {
        List<IPropertyDesc<Node, IPropertyObjectWorld>> lst = new ArrayList<IPropertyDesc<Node,IPropertyObjectWorld>>();
        if (this.propertyModel != null) {
            IPropertyDesc<Node, IPropertyObjectWorld>[] descs = this.propertyModel.getPropertyDescs();
            for (IPropertyDesc<Node, IPropertyObjectWorld> pd : descs) {
                Object value = this.propertyModel.getPropertyValue(pd.getId());
                if (value instanceof ValueSpread) {
                    ValueSpread vs = (ValueSpread) value;
                    if (vs.isAnimated()) {
                        lst.add(pd);
                    }
                }
            }
        }

        input = (IPropertyDesc<Node, IPropertyObjectWorld>[]) lst.toArray(new IPropertyDesc<?, ?>[lst.size()]);

        if (!Arrays.equals(input, oldInput)) {
            this.view.setInput(input);
        }

        oldInput = input;
        this.view.refresh();
    }

    public IPropertyDesc<Node, ? extends IPropertyObjectWorld>[] getInput() {
        return this.input;
    }

    private HermiteSpline getCurve(int index) {
        ValueSpread vs = (ValueSpread)this.propertyModel.getPropertyValue(this.input[index].getId());
        return (HermiteSpline)vs.getCurve();
    }

    private int getSingleCurveIndexFromSelection() {
        if (!selection.isEmpty() && selection instanceof IStructuredSelection) {
            int index = -1;
            if (selection instanceof ITreeSelection) {
                ITreeSelection tree = (ITreeSelection)selection;
                for (TreePath path : tree.getPaths()) {
                    int i = (Integer)path.getSegment(0);
                    if (index == -1) {
                        index = i;
                    } else if (index != i) {
                        return -1;
                    }
                }
            }
            return index;
        }
        return -1;
    }

    private int[] getSinglePointIndexFromSelection() {
        if (!selection.isEmpty() && selection instanceof IStructuredSelection) {
            if (selection instanceof ITreeSelection) {
                ITreeSelection tree = (ITreeSelection)selection;
                TreePath[] paths = tree.getPaths();
                if (paths.length == 1 && paths[0].getSegmentCount() == 2) {
                    return new int[] {(Integer)paths[0].getSegment(0), (Integer)paths[0].getSegment(1)};
                }
            }
        }
        return null;
    }

    private Map<Integer, List<Integer>> getPointsMapFromSelection() {
        Map<Integer, List<Integer>> points = new HashMap<Integer, List<Integer>>();
        if (!selection.isEmpty() && selection instanceof IStructuredSelection) {
            if (selection instanceof ITreeSelection) {
                ITreeSelection tree = (ITreeSelection)selection;
                for (TreePath path : tree.getPaths()) {
                    int curveIndex = (Integer)path.getSegment(0);
                    List<Integer> list = points.get(curveIndex);
                    if (list == null) {
                        list = new ArrayList<Integer>();
                        points.put(curveIndex, list);
                    }
                    if (path.getSegmentCount() > 1) {
                        int pointIndex = (Integer)path.getSegment(1);
                        points.get(curveIndex).add(pointIndex);
                    }
                }
                // Sort point indices
                for (Map.Entry<Integer, List<Integer>> entry : points.entrySet()) {
                    Collections.sort(entry.getValue());
                }
            }
        }
        return points;
    }

    private int[][] getPointsListFromSelection() {
        List<int[]> points = new ArrayList<int[]>();
        if (!selection.isEmpty() && selection instanceof IStructuredSelection) {
            if (selection instanceof ITreeSelection) {
                ITreeSelection tree = (ITreeSelection)selection;
                for (TreePath path : tree.getPaths()) {
                    if (path.getSegmentCount() > 1) {
                        int curveIndex = (Integer)path.getSegment(0);
                        int pointIndex = (Integer)path.getSegment(1);
                        points.add(new int[] {curveIndex, pointIndex});
                    }
                }
            }
        }
        return points.toArray(new int[points.size()][]);
    }

    private IUndoableOperation setCurve(int index, HermiteSpline spline) {
        String id = this.input[index].getId();
        ValueSpread vs = (ValueSpread)this.propertyModel.getPropertyValue(id);
        vs.setCurve(spline);
        return this.propertyModel.setPropertyValue(this.input[index].getId(), vs, true);
    }

    private IUndoableOperation setCurves(Map<Integer, HermiteSpline> curves, boolean force) {
        int curveCount = curves.size();
        Object[] ids = new Object[curveCount];
        Object[] values = new Object[curveCount];
        int index = 0;
        for (Map.Entry<Integer, HermiteSpline> entry : curves.entrySet()) {
            String id = this.input[entry.getKey()].getId();
            ids[index] = id;
            ValueSpread vs = (ValueSpread)this.propertyModel.getPropertyValue(id);
            vs.setCurve(entry.getValue());
            values[index] = vs;
            ++index;
        }
        return PropertyUtil.setProperties(this.propertyModel, ids, values, force);
    }

    private void execute(IUndoableOperation operation) {
        operation.addContext(this.undoContext);
        try {
            IStatus status = this.history.execute(operation, null, null);
            if (status != Status.OK_STATUS) {
                throw new RuntimeException("Failed to execute operation");
            }
        } catch (final ExecutionException e) {
            throw new RuntimeException("Failed to execute operation", e);
        }
    }

    private int[][] findPoints(Point2d min, Point2d max) {
        Vector2d delta = new Vector2d(max);
        delta.sub(min);
        List<int[]> points = new ArrayList<int[]>();
        // Select points
        for (int i = 0; i < this.input.length; ++i) {
            HermiteSpline spline = getCurve(i);
            for (int j = 0; j < spline.getCount(); ++j) {
                SplinePoint point = spline.getPoint(j);
                if (min.x <= point.getX() && point.getX() <= max.x
                        && min.y <= point.getY() && point.getY() <= max.y) {
                    points.add(new int[] {i, j});
                }
            }
        }
        return points.toArray(new int[points.size()][]);
    }

    /**
     * NOTE this function only works for relatively small boxes
     */
    private int findClosestCurve(Point2d min, Point2d max) {
        int closestIndex = -1;
        double closestDistance = Double.MAX_VALUE;
        for (int i = 0; i < this.input.length; ++i) {
            HermiteSpline spline = getCurve(i);
            Point2d p0 = new Point2d(min.getX(), spline.getY(min.getX()));
            Vector2d delta = new Vector2d(max.getX(), spline.getY(max.getX()));
            delta.sub(p0);
            Vector2d dir = new Vector2d(delta);
            dir.normalize();
            Vector2d hitDelta = new Vector2d(this.dragStart);
            hitDelta.sub(p0);
            double t = hitDelta.dot(dir);
            t = Math.max(0.0, Math.min(t, delta.length()));
            hitDelta.scale(t, dir);
            Point2d closestCurvePosition = new Point2d(hitDelta);
            closestCurvePosition.add(p0);
            if (hitPosition(this.dragStart, closestCurvePosition, this.hitBoxExtents)) {
                double distance = closestCurvePosition.distance(this.dragStart);
                if (distance < closestDistance) {
                    closestIndex = i;
                }
            }
        }
        return closestIndex;
    }

    private TreeSelection select(int[][] points) {
        List<TreePath> selection = new ArrayList<TreePath>(points.length);
        for (int i = 0; i < points.length; ++i) {
            Integer[] value = new Integer[points[i].length];
            for (int j = 0; j < value.length; ++j) {
                value[j] = points[i][j];
            }
            selection.add(new TreePath(value));
        }
        return new TreeSelection(selection.toArray(new TreePath[points.length]));
    }

    private boolean hitPosition(Point2d position, Point2d hitPosition, Vector2d hitBoxExtents) {
        Vector2d delta = new Vector2d();
        delta.sub(position, hitPosition);
        return Math.abs(delta.getX()) <= hitBoxExtents.getX()
                && Math.abs(delta.getY()) <= hitBoxExtents.getY();
    }

    private boolean hitTangent(Point2d position, SplinePoint point, Vector2d hitBoxExtents, Vector2d screenScale, double screenTangentLength) {
        Point2d pointPosition = new Point2d(point.getX(), point.getY());
        Vector2d screenTangent = new Vector2d(point.getTx() * screenScale.getX(), point.getTy() * screenScale.getY());
        screenTangent.normalize();
        screenTangent.scale(screenTangentLength);
        Vector2d tangent = new Vector2d(screenTangent.getX() / screenScale.getX(), screenTangent.getY() / screenScale.getY());
        Point2d t0 = new Point2d(tangent);
        Point2d t1 = new Point2d(tangent);
        t0.scaleAdd(1.0, pointPosition);
        t1.scaleAdd(-1.0, pointPosition);
        return hitPosition(position, t0, hitBoxExtents)
                || hitPosition(position, t1, hitBoxExtents);
    }

    private void startMoveSelection() {
        this.originalPositions.clear();
        int[][] selectedPoints = getPointsListFromSelection();
        for (int[] indices : selectedPoints) {
            HermiteSpline spline = getCurve(indices[0]);
            SplinePoint point = spline.getPoint(indices[1]);
            this.originalPositions.add(new Point2d(point.getX(), point.getY()));
        }
        this.dragMode = DragMode.MOVE_POINTS;
    }

    private int findPoint(HermiteSpline spline, double x) {
        int pointIndex = -1;
        int pointCount = spline.getCount();
        x = Math.max(0, Math.min(1, x));
        for (int i = 0; i < pointCount; ++i) {
            SplinePoint p = spline.getPoint(i);
            if (Math.abs(p.getX() - x) < HermiteSpline.MIN_POINT_X_DISTANCE) {
                return i;
            }
        }
        return pointIndex;
    }

    @Override
    public void onAddPoint(Point2d p) {
        int index = getSingleCurveIndexFromSelection();
        if (index >= 0) {
            HermiteSpline spline = getCurve(index);
            // Find existing points
            int pointIndex = findPoint(spline, p.getX());
            if (pointIndex >= 0) {
                setSelection(select(new int[][] {{index, pointIndex}}));
                this.view.refresh();
            } else {
                spline = spline.insertPoint(p.x, p.y);
                pointIndex = findPoint(spline, p.getX());
                ISelection newSelection = select(new int[][] {{index, pointIndex}});
                execute(new InsertPointOperation(setCurve(index, spline), this.selection, newSelection, this));
            }
        }
    }

    @Override
    public void onRemove() {
        Map<Integer, List<Integer>> selectedPoints = getPointsMapFromSelection();
        Map<Integer, HermiteSpline> curves = new HashMap<Integer, HermiteSpline>();
        if (!selectedPoints.isEmpty()) {
            List<int[]> newSelectedPoints = new ArrayList<int[]>();
            for (Map.Entry<Integer, List<Integer>> entry : selectedPoints.entrySet()) {
                int curveIndex = entry.getKey();
                HermiteSpline spline = getCurve(curveIndex);
                List<Integer> points = entry.getValue();
                Collections.sort(points);
                int pointCount = points.size();
                int removeCount = 0;
                for (int i = 0; i < pointCount; ++i) {
                    int pointIndex = points.get(i) - removeCount;
                    int preCount = spline.getCount();
                    spline = spline.removePoint(pointIndex);
                    if (preCount == spline.getCount()) {
                        newSelectedPoints.add(new int[] {curveIndex, pointIndex});
                    } else {
                        ++removeCount;
                    }
                }
                curves.put(curveIndex, spline);
            }
            int[][] selection = null;
            if (!newSelectedPoints.isEmpty()) {
                selection = newSelectedPoints.toArray(new int[newSelectedPoints.size()][]);
            } else {
                Integer[] selectedCurves = selectedPoints.keySet().toArray(new Integer[selectedPoints.size()]);
                selection = new int[selectedCurves.length][];
                for (int i = 0; i < selectedCurves.length; ++i) {
                    selection[i] = new int[] {selectedCurves[i]};
                }
            }
            ISelection newSelection = select(selection);
            execute(new RemovePointsOperation(setCurves(curves, true), this.selection, newSelection, this));
        }
    }

    @Override
    public void onStartDrag(Point2d start, Vector2d screenScale, double screenDragPadding, double screenHitPadding, double screenTangentLength) {
        Vector2d invScreenScale = new Vector2d(1.0 / screenScale.getX(), 1.0 / screenScale.getY());
        invScreenScale.absolute();
        this.dragStart.set(start);
        this.dragMode = DragMode.SELECT;
        this.originalSelection = this.selection;
        this.minDragExtents.scale(screenDragPadding, invScreenScale);
        this.hitBoxExtents.scale(screenHitPadding, invScreenScale);
        this.dragging = false;

        Point2d min = new Point2d(hitBoxExtents);
        min.negate();
        min.add(start);
        Point2d max = new Point2d(hitBoxExtents);
        max.add(start);
        int[][] points = findPoints(min, max);
        if (!selection.isEmpty()) {
            int[] indices = getSinglePointIndexFromSelection();
            // Check for normals hit
            if (indices != null) {
                HermiteSpline spline = getCurve(indices[0]);
                SplinePoint point = spline.getPoint(indices[1]);
                if (hitTangent(start, point, hitBoxExtents, screenScale, screenTangentLength)) {
                    this.dragMode = DragMode.SET_TANGENT;
                    return;
                }
            }
            // Check for points to move
            ITreeSelection treeSelection = (ITreeSelection)selection;
            TreePath[] paths = treeSelection.getPaths();
            for (int i = 0; i < paths.length; ++i) {
                TreePath path = paths[i];
                if (path.getSegmentCount() > 1) {
                    int curveIndex = (Integer)path.getSegment(0);
                    HermiteSpline spline = getCurve(curveIndex);
                    int pointIndex = (Integer)path.getSegment(1);
                    SplinePoint point = spline.getPoint(pointIndex);
                    Point2d pointPosition = new Point2d(point.getX(), point.getY());
                    if (hitPosition(start, pointPosition, hitBoxExtents)) {
                        startMoveSelection();
                        return;
                    }
                }
            }
        }
        // Select and move hit points
        if (points.length > 0) {
            int curveIndex = getSingleCurveIndexFromSelection();
            // Prioritize points from selected curve
            if (curveIndex >= 0) {
                for (int i = 0; i < points.length; ++i) {
                    if (points[i][0] == curveIndex) {
                        setSelection(select(new int[][] {points[i]}));
                    }
                }
            } else {
                setSelection(select(points));
            }
            startMoveSelection();
        } else {
            int curveIndex = findClosestCurve(min, max);
            if (curveIndex >= 0) {
                setSelection(select(new int[][] {{curveIndex}}));
            } else {
                setSelection(select(new int[][] {}));
            }
        }
        this.view.refresh();
    }

    @Override
    public void onDrag(Point2d position) {
        Vector2d delta = new Vector2d();
        delta.sub(position, this.dragStart);
        if (this.dragging || (Math.abs(delta.getX()) >= this.minDragExtents.getX() || Math.abs(delta.getY()) >= this.minDragExtents.getY())) {
            boolean initialDrag = !this.dragging;
            this.dragging = true;
            switch (this.dragMode) {
            case MOVE_POINTS:
                int[][] selectedPoints = getPointsListFromSelection();
                int pointCount = selectedPoints.length;
                Point2d p = new Point2d();
                Map<Integer, HermiteSpline> curves = new HashMap<Integer, HermiteSpline>();
                for (int i = 0; i < pointCount; ++i) {
                    int[] indices = selectedPoints[i];
                    int curveIndex = indices[0];
                    int pointIndex = indices[1];
                    HermiteSpline spline = curves.get(curveIndex);
                    if (spline == null) {
                        spline = getCurve(curveIndex);
                    }
                    p.set(this.originalPositions.get(i));
                    p.add(delta);
                    spline = spline.setPosition(pointIndex, p.getX(), p.getY());
                    curves.put(curveIndex, spline);
                }
                IUndoableOperation op = setCurves(curves, true);
                if (initialDrag) {
                    MovePointsOperation moveOp = new MovePointsOperation(op);
                    moveOp.setType(Type.OPEN);
                    execute(moveOp);
                } else {
                    ((IMergeableOperation)op).setType(Type.INTERMEDIATE);
                    execute(op);
                }
                break;
            case SET_TANGENT:
                int[] indices = getSinglePointIndexFromSelection();
                if (indices != null) {
                    int curveIndex = indices[0];
                    int pointIndex = indices[1];
                    HermiteSpline spline = getCurve(curveIndex);
                    SplinePoint point = spline.getPoint(pointIndex);
                    Vector2d tangent = new Vector2d(point.getX(), point.getY());
                    tangent.sub(position);
                    if (tangent.lengthSquared() > 0.0) {
                        if (tangent.getX() < 0.0) {
                            tangent.negate();
                        }
                        tangent.normalize();
                        spline = spline.setTangent(pointIndex, tangent.x, tangent.y);
                        IUndoableOperation tangentOp = setCurve(curveIndex, spline);
                        if (initialDrag) {
                            SetTangentOperation setTangentOp = new SetTangentOperation(tangentOp);
                            setTangentOp.setType(Type.OPEN);
                            execute(setTangentOp);
                        } else {
                            ((IMergeableOperation)tangentOp).setType(Type.INTERMEDIATE);
                            execute(tangentOp);
                        }
                    }
                } else {
                    throw new IllegalStateException("No single point selected.");
                }
                break;
            case SELECT:
                Point2d min = new Point2d(Math.min(this.dragStart.x, position.x), Math.min(this.dragStart.y, position.y));
                Point2d max = new Point2d(Math.max(this.dragStart.x, position.x), Math.max(this.dragStart.y, position.y));
                this.view.setSelectionBox(min, max);
                min.sub(this.hitBoxExtents);
                max.add(this.hitBoxExtents);
                int[][] points = findPoints(min, max);
                setSelection(select(points));
                break;
            }
            this.view.refresh();
        }
    }

    @Override
    public void onEndDrag() {
        if (this.dragging) {
            switch (this.dragMode) {
            case MOVE_POINTS:
                int[][] selectedPoints = getPointsListFromSelection();
                int pointCount = selectedPoints.length;
                Map<Integer, HermiteSpline> curves = new HashMap<Integer, HermiteSpline>();
                for (int i = 0; i < pointCount; ++i) {
                    int[] indices = selectedPoints[i];
                    int curveIndex = indices[0];
                    HermiteSpline spline = getCurve(curveIndex);
                    curves.put(curveIndex, spline);
                }
                IUndoableOperation op = setCurves(curves, true);
                execute(op);
                break;
            case SET_TANGENT:
                int[] indices = getSinglePointIndexFromSelection();
                if (indices != null) {
                    int curveIndex = indices[0];
                    HermiteSpline spline = getCurve(curveIndex);
                    IUndoableOperation tangentOp = setCurve(curveIndex, spline);
                    ((IMergeableOperation)tangentOp).setType(Type.CLOSE);
                    execute(tangentOp);
                }
                break;
            case SELECT:
                this.view.setSelectionBox(new Point2d(), new Point2d());
                break;
            }
            this.originalSelection = null;
        }
        this.view.refresh();
    }

    @Override
    public void onSelectAll() {
        List<int[]> points = new ArrayList<int[]>();
        int curveCount = this.input.length;
        for (int i = 0; i < curveCount; ++i) {
            HermiteSpline spline = getCurve(i);
            int pointCount = spline.getCount();
            for (int j = 0; j < pointCount; ++j) {
                points.add(new int[] {i, j});
            }
        }
        setSelection(select(points.toArray(new int[points.size()][])));
    }

    @Override
    public void onDeselectAll() {
        setSelection(select(new int[][] {}));
    }

}
