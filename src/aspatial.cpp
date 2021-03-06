/** @file aspatial.cpp
 * @brief Implements spatial discretizations.
 * @author Aditya Kashi
 * @date 2016-02-24
 */

#include "aspatial.hpp"

namespace acfd {

template <short nvars>
SpatialBase<nvars>::SpatialBase(const UMesh2dh* mesh, const int _p_degree, char basistype)
	: m(mesh), p_degree(_p_degree), basis_type(basistype)
{
	std::cout << " SpatialBase: Setting up spatal integrator for FE polynomial degree " << p_degree << std::endl;

	// set quadrature strength
	//int dom_quaddegree = 2*p_degree + m->degree()-1;
	//int boun_quaddegree = 2*p_degree + m->degree()-1;
	int dom_quaddegree = 2*p_degree;
	int boun_quaddegree = 2*p_degree;
	if(dom_quaddegree == 0) dom_quaddegree = 1;
	if(boun_quaddegree == 0) boun_quaddegree = 1;
	std::cout << " SpatialBase: Quadrature strengths for domain and boundary integrals set at " << dom_quaddegree << ", " << boun_quaddegree << std::endl;

	dtquad = new Quadrature2DTriangle();
	dtquad->initialize(dom_quaddegree);
	dsquad = new Quadrature2DSquare();
	dsquad->initialize(dom_quaddegree);
	bquad = new Quadrature1D();
	bquad->initialize(boun_quaddegree);

	map2d = new LagrangeMapping2D[m->gnelem()];
	elems = new Element*[m->gnelem()];
	if(basistype == 't') {
		for(int iel = 0; iel < m->gnelem(); iel++) {
			elems[iel] = new TaylorElement();
		}
	}
	else {
		/*elems[0] = new LagrangeElement();
		for(int iel = 1; iel < m->gnelem(); iel++)
			elems[iel] = elems[0];*/
		for(int iel = 0; iel < m->gnelem(); iel++) {
			elems[iel] = new LagrangeElement();
		}
	}

	dummyelem = new DummyElement();

	map1d = new LagrangeMapping1D[m->gnaface()];
	faces = new FaceElement[m->gnaface()];
}

template <short nvars>
SpatialBase<nvars>::~SpatialBase()
{
	delete dtquad;
	delete dsquad;
	delete bquad;
	delete [] map2d;
	delete [] map1d;
	delete [] faces;
	for(int iel = 0; iel < m->gnelem(); iel++)
		delete elems[iel];
	delete [] elems;
	delete dummyelem;
}

template <short nvars>
void SpatialBase<nvars>::computeFEData()
{
	minv.resize(m->gnelem());
	ntotaldofs = 0;

	// loop over elements to setup maps and elements and compute mass matrices
	for(int iel = 0; iel < m->gnelem(); iel++)
	{
		Matrix phynodes(NDIM,m->gnnode(iel));
		for(int i = 0; i < m->gnnode(iel); i++)
			for(int j = 0; j < NDIM; j++)
				phynodes(j,i) = m->gcoords(m->ginpoel(iel,i),j);

		if(m->gnnode(iel) == 4 || m->gnnode(iel) == 9 || m->gnnode(iel) == 16)
			map2d[iel].setAll(m->degree(), phynodes, dsquad);
		else {
			map2d[iel].setAll(m->degree(), phynodes, dtquad);
		}

		elems[iel]->initialize(p_degree, &map2d[iel]);
		ntotaldofs += elems[iel]->getNumDOFs();

		// allocate mass matrix
		minv[iel] = Matrix::Zero(elems[iel]->getNumDOFs(), elems[iel]->getNumDOFs());

		// compute mass matrix
		for(int ig = 0; ig < map2d[iel].getQuadrature()->numGauss(); ig++)
		{
			a_real weightandjdet = map2d[iel].jacDet()[ig] * map2d[iel].getQuadrature()->weights()(ig);
			for(int idof = 0; idof < elems[iel]->getNumDOFs(); idof++)
				for(int jdof = 0; jdof < elems[iel]->getNumDOFs(); jdof++)
					minv[iel](idof,jdof) += elems[iel]->bFunc()(ig,idof)*elems[iel]->bFunc()(ig,jdof) * weightandjdet;
		}

		minv[iel] = minv[iel].inverse().eval();

		/** \note Computation of physical coordinates of domain quadrature points is required separately for Lagrange elements
		 * only for the purpose of computing source term contributions and errors.
		 */
		if(basis_type == 'l')
			map2d[iel].computePhysicalCoordsOfDomainQuadraturePoints();
	}
	std::printf(" SpatialBase: computeFEData: Total number of DOFs = %d\n", ntotaldofs);

	dummyelem->initialize(p_degree, &map2d[0]);

	// loop over faces
	for(int iface = 0; iface < m->gnbface(); iface++)
	{
		int lelem = m->gintfac(iface,0);
		Matrix phynodes(NDIM, m->gnnofa(iface));
		for(int i = 0; i < m->gnnofa(iface); i++)
			for(int j = 0; j < NDIM; j++)
				phynodes(j,i) = m->gcoords(m->gintfac(iface,2+i),j);

		map1d[iface].setAll(m->degree(), phynodes, bquad);
		map1d[iface].computeAll();

		faces[iface].initialize(elems[lelem], dummyelem, &map1d[iface], m->gfacelocalnum(iface,0), m->gfacelocalnum(iface,1));
	}

	for(int iface = m->gnbface(); iface < m->gnaface(); iface++)
	{
		int lelem = m->gintfac(iface,0);
		int relem = m->gintfac(iface,1);
		Matrix phynodes(NDIM, m->gnnofa(iface));
		for(int i = 0; i < m->gnnofa(iface); i++)
			for(int j = 0; j < NDIM; j++)
				phynodes(j,i) = m->gcoords(m->gintfac(iface,2+i),j);

		map1d[iface].setAll(m->degree(), phynodes, bquad);
		map1d[iface].computeAll();

		faces[iface].initialize(elems[lelem], elems[relem], &map1d[iface], m->gfacelocalnum(iface,0), m->gfacelocalnum(iface,1));
		/*std::cout << "  SpatialBase: facelocalnum: L elem " << lelem+m->gnface()+1 << ", R elem " << relem+m->gnface()+1
			<< ": " << m->gfacelocalnum(iface,0) << ", " << m->gfacelocalnum(iface,1) << std::endl;*/
	}

	std::cout << " SpatialBase: computeFEData: Mesh degree = " << m->degree() << ", geom map degee = " << map2d[0].getDegree()
		 << ", element degree = " << elems[0]->getDegree() << std::endl;
}

template <short nvars>
void SpatialBase<nvars>::spatialSetup(std::vector<Matrix>& u, std::vector<Matrix>& res, std::vector<a_real>& mets)
{
	computeFEData();
	
	// allocate
	u.resize(m->gnelem());
	res.resize(m->gnelem());
	mets.resize(m->gnelem());
	for(a_int iel = 0; iel < m->gnelem(); iel++)
	{
		u[iel].resize(nvars, elems[iel]->getNumDOFs());
		res[iel].resize(nvars, elems[iel]->getNumDOFs());
	}
}

template <short nvars>
a_real SpatialBase<nvars>::computeElemL2Norm2(const int ielem, const Vector& __restrict__ ug) const
{
	int ndofs = elems[ielem]->getNumDOFs();
	a_real l2error = 0;

	const Matrix& bfunc = elems[ielem]->bFunc();
	const GeomMapping2D* gmap = elems[ielem]->getGeometricMapping();
	int ng = gmap->getQuadrature()->numGauss();
	const amat::Array2d<a_real>& wts = gmap->getQuadrature()->weights();

	for(int ig = 0; ig < ng; ig++)
	{
		a_real lu = 0;
		for(int j = 0; j < ndofs; j++) {
			lu += ug(j)*bfunc(ig,j);
		}
		l2error += lu*lu * wts(ig) * gmap->jacDet()[ig];
	}

	return l2error;
}

template <short nvars>
a_real SpatialBase<nvars>::computeL2Norm(const std::vector<Matrix> w, const int comp) const
{
	a_real l2norm = 0;
	for(int ielem = 0; ielem < m->gnelem(); ielem++)
	{
		const GeomMapping2D* gmap = elems[ielem]->getGeometricMapping();
		int ng = gmap->getQuadrature()->numGauss();
		const amat::Array2d<a_real>& wts = gmap->getQuadrature()->weights();
		Vector vals(ng);
		elems[ielem]->interpolateComponent(comp,w[ielem],vals);

		for(int ig = 0; ig < ng; ig++)
		{
			l2norm += vals(ig)*vals(ig) * wts(ig) * gmap->jacDet()[ig];
		}
	}

	return std::sqrt(l2norm);
}

template <short nvars>
a_real SpatialBase<nvars>::computeElemL2Error2(const int ielem, const int comp,
	const Matrix& __restrict__ ug, a_real (* const exact)(a_real, a_real, a_real), const double time) const
{
	int ndofs = elems[ielem]->getNumDOFs();
	a_real l2error = 0;

	const Matrix& bfunc = elems[ielem]->bFunc();
	const GeomMapping2D* gmap = elems[ielem]->getGeometricMapping();
	int ng = gmap->getQuadrature()->numGauss();
	const amat::Array2d<a_real>& wts = gmap->getQuadrature()->weights();
	const Matrix& qp = gmap->map();

	// TODO: Use interpolateAll here
	for(int ig = 0; ig < ng; ig++)
	{
		a_real lu = 0;
		for(int j = 0; j < ndofs; j++) {
			lu += ug(comp,j)*bfunc(ig,j);
		}
		l2error += std::pow(lu-exact(qp(ig,0),qp(ig,1),time),2) * wts(ig) * gmap->jacDet()[ig];
	}

	return l2error;
}

template <short nvars>
a_real SpatialBase<nvars>::computeL2Error(double (*const exact)(double,double,double), const double time, const std::vector<Matrix>& u) const
{
	double l2error = 0;
	for(int iel = 0; iel < m->gnelem(); iel++)
	{
		l2error += computeElemL2Error2(iel, 0, u[iel], exact, time);
	}
	return sqrt(l2error);
}

template <short nvars>
void SpatialBase<nvars>::setInitialConditionNodal(const int comp, double (**const init)(a_real, a_real), std::vector<Matrix>& u)
{
	if(basis_type != 'l') {
		printf("!  SpatialBase: setInitialConditionNodal: Not nodal basis!\n");
		return;
	}
	for(int iel = 0; iel < m->gnelem(); iel++)
	{
		Matrix refs = reinterpret_cast<LagrangeElement*>(elems[iel])->getReferenceNodes();
		Matrix points(elems[iel]->getNumDOFs(), NDIM);
		map2d[iel].calculateMap(refs, points);

		for(int idof = 0; idof < elems[iel]->getNumDOFs(); idof++) {
			u[iel](comp,idof) = init[0](points(idof,0),points(idof,1));
		}
	}
}

template <short nvars>
void SpatialBase<nvars>::setInitialConditionModal(const int comp, double (**const init)(a_real, a_real), std::vector<Matrix>& u)
{
	if(basis_type != 't') {
		printf("!  SpatialBase: setInitialConditionModal: Not Taylor basis!\n");
		return;
	}
	for(int iel = 0; iel < m->gnelem(); iel++)
	{
		TaylorElement* elem = reinterpret_cast<TaylorElement*>(elems[iel]);
		a_real xc = elem->getCenter()[0], yc = elem->getCenter()[1];
		a_real dx = elem->getDelta()[0], dy = elem->getDelta()[1];

		u[iel](comp,0) = init[0](xc,yc);
		if(p_degree >= 1) {
			u[iel](comp,1) = init[1](xc,yc)*dx;
			u[iel](comp,2) = init[2](xc,yc)*dx;
		}
		if(p_degree >= 2) {
			u[iel](comp,3) = init[3](xc,yc)*2*dx*dx;
			u[iel](comp,4) = init[4](xc,yc)*2*dy*dy;
			u[iel](comp,5) = init[5](xc,yc)*dx*dy;
		}
	}
}

template <short nvars>
void SpatialBase<nvars>::add_source( a_real (*const rhs)(a_real, a_real, a_real), a_real t, std::vector<Matrix>& res) { }

template class SpatialBase<1>;

}	// end namespace
