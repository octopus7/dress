"""Detect crossings between the two actual simulated, open garment surfaces."""
import json,sys,unittest
from pathlib import Path
from mathutils.bvhtree import BVHTree
sys.path.insert(0,str(Path(__file__).parent))
from validate_cloth_surface import read_obj

def crossings(a,b):
    return BVHTree.FromPolygons(*a,all_triangles=True,epsilon=0).overlap(
        BVHTree.FromPolygons(*b,all_triangles=True,epsilon=0))

class LayerTests(unittest.TestCase):
    def test_crossing_and_separated_open_triangles(self):
        a=([(0,0,0),(2,0,0),(0,2,0)],[(0,1,2)])
        self.assertTrue(crossings(a,([(0.5,0.5,-1),(0.5,0.5,1),(1,1,1)],[(0,1,2)])))
        self.assertFalse(crossings(a,([(0,0,.05),(2,0,.05),(0,2,.05)],[(0,1,2)])))

if __name__=='__main__':
    args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
    if not args:
        result=unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(LayerTests))
        if not result.wasSuccessful():raise RuntimeError('Layer checker failed')
    else:
        folder=Path(args[0]);poses=[]
        for pose in range(4):
            pairs=crossings(read_obj(folder/f'Pose{pose}_Top.obj'),read_obj(folder/f'Pose{pose}_Bottom.obj'))
            poses.append({'pose':pose,'intersecting_triangle_pairs':len(pairs),'pairs':pairs})
            print('POSE',pose,'INTERSECTIONS',len(pairs),flush=True)
        report={'passed':all(p['intersecting_triangle_pairs']==0 for p in poses),'poses':poses}
        (folder/'layer-validation.json').write_text(json.dumps(report,indent=2))
        if not report['passed']:raise RuntimeError('Garment surfaces cross')
